# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# Note: This will be a simple addon later, but until it gets to master, it's simpler to have it
#       as a startup module!

import bpy
from bpy.types import AssetEngine, Panel
from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        FloatProperty,
        EnumProperty,
        CollectionProperty,
        )

import binascii
import concurrent.futures as futures
import hashlib
import json
import os
import stat
import time

AMBER_DB_NAME = "__amber_db.json"
AMBER_DBK_VERSION = "version"


##########
# Helpers.

# Notes about UUIDs:
#    * UUID of an asset/variant/revision is computed once at its creation! Later changes to data do not affect it.
#    * Collision, for unlikely it is, may happen across different repositories...
#      Doubt this will be practical issue though.
#    * We keep eight first bytes of 'clear' identifier, to (try to) keep some readable uuid.

def _uuid_gen_single(used_uuids, uuid_root, h, str_arg):
    h.update(str_arg.encode())
    uuid = uuid_root + h.digest()
    uuid = uuid[:23].replace(b'\0', b'\1')  # No null chars, RNA 'bytes' use them as in regular strings... :/
    if uuid not in used_uuids:  # *Very* likely, but...
        used_uuids.add(uuid)
        return uuid
    return None


def _uuid_gen(used_uuids, uuid_root, bytes_seed, *str_args):
    h = hashlib.md5(bytes_seed)
    for arg in str_args:
        uuid = _uuid_gen_single(used_uuids, uuid_root, h, arg)
        if uuid is not None:
            return uuid
    # This is a fallback in case we'd get a collision... Should never be needed in real life!
    for i in range(100000):
        uuid = _uuid_gen_single(used_uuids, uuid_root, h, i.to_bytes(4, 'little'))
        if uuid is not None:
            return uuid
    return None  # If this happens...


def uuid_asset_gen(used_uuids, path_db, name, tags):
    uuid_root = name.encode()[:8] + b'|'
    return _uuid_gen_single(used_uuids, uuid_root, path_db.encode(), name, *tags)


def uuid_variant_gen(used_uuids, asset_uuid, name):
    uuid_root = name.encode()[:8] + b'|'
    return _uuid_gen_single(used_uuids, uuid_root, asset_uuid, name)


def uuid_revision_gen(used_uuids, variant_uuid, number, size, time):
    uuid_root = str(number).encode() + b'|'
    return _uuid_gen_single(used_uuids, uuid_root, variant_uuid, str(number), str(size), str(timestamp))


#############
# Amber Jobs.
class AmberJob:
    def __init__(self, executor, job_id):
        self.executor = executor
        self.job_id = job_id
        self.status = {'VALID'}
        self.progress = 0.0


class AmberJobList(AmberJob):
    @staticmethod
    def ls_repo(db_path):
        repo = None
        with open(db_path, 'r') as db_f:
            repo = json.load(db_f)
        if isinstance(repo, dict):
            repo_ver = repo.get(AMBER_DBK_VERSION, "")
            if repo_ver != "1.0.0":
                # Unsupported...
                print("WARNING: unsupported Amber repository version '%s'." % repo_ver)
                repo = None
        else:
            repo = None
        return repo

    @staticmethod
    def ls(path):
        repo = None
        ret = [".."]
        tmp = os.listdir(path)
        if AMBER_DB_NAME in tmp:
            # That dir is an Amber repo, we only list content define by our amber 'db'.
            repo = AmberJobList.ls_repo(os.path.join(path, AMBER_DB_NAME))
        if repo is None:
            ret += tmp
        #~ time.sleep(0.1)  # 100% Artificial Lag (c)
        return ret, repo

    @staticmethod
    def stat(root, path):
        st = os.lstat(root + path)
        #~ time.sleep(0.1)  # 100% Artificial Lag (c)
        return path, (stat.S_ISDIR(st.st_mode), st.st_size, st.st_mtime)

    def start(self):
        self.nbr = 0
        self.tot = 0
        self.ls_task = self.executor.submit(self.ls, self.root)
        self.status = {'VALID', 'RUNNING'}

    def update(self, repository, dirs, uuids):
        self.status = {'VALID', 'RUNNING'}
        if self.ls_task is not None:
            if not self.ls_task.done():
                return
            paths, repo = self.ls_task.result()
            self.ls_task = None
            self.tot = len(paths)
            repository.clear()
            dirs.clear()
            if repo is not None:
                repository.update(repo)
            for p in paths:
                self.stat_tasks.add(self.executor.submit(self.stat, self.root, p))

        done = set()
        for tsk in self.stat_tasks:
            if tsk.done():
                path, (is_dir, size, timestamp) = tsk.result()
                self.nbr += 1
                if is_dir:
                    # We only list dirs from real file system.
                    dirs.append((path, size, timestamp, path.encode()[:8] + b"|" + bytes(self.nbr)))
                done.add(tsk)
        self.stat_tasks -= done

        self.progress = self.nbr / self.tot
        if not self.stat_tasks and self.ls_task is None:
            self.status = {'VALID'}

    def __init__(self, executor, job_id, root):
        super().__init__(executor, job_id)
        self.root = root

        self.ls_task = None
        self.stat_tasks = set()

        self.start()

    def __del__(self):
        # Avoid useless work!
        if self.ls_task is not None:
            self.ls_task.cancel()
        for tsk in self.stat_tasks:
            tsk.cancel()


###########################
# Main Asset Engine class.
class AssetEngineAmber(AssetEngine):
    bl_label = "Amber"

    max_entries = IntProperty(
            name="Max Entries",
            description="Max number of entries to return as a 'list' request (avoids risks of 'explosion' on big repos)",
            min=10, max=10000, default=1000,
    )

    # *Very* primitive! Only 32 tags allowed...
    def _tags_gen(self, context):
        tags = getattr(self, "tags_source", [])
        return [(tag, tag, str(prio)) for tag, prio in tags[:32]]
    tags = EnumProperty(
            items=_tags_gen,
            name="Tags",
            description="Active tags",
            options={'ENUM_FLAG'},
    )

    def __init__(self):
        self.executor = futures.ThreadPoolExecutor(8)  # Using threads for now, if issues arise we'll switch to process.
        self.jobs = {}
        self.root = ""
        self.uuids = {}
        self.repo = {}
        self.dirs = []
        self.tags_source = []

        self.job_uuid = 1

    def __del__(self):
        pass
        # XXX This errors, saying self has no executor attribute... Suspect some py/RNA funky game. :/
        #     Even though it does not seem to be an issue, this is not nice and shall be fixed somehow.
        #~ self.executor.shutdown(wait=False)

    def fill_entries(self, entries):
        if entries.root_path != self.root:
            entries.entries.clear()
            self.root = entries.root_path

        existing = {}
        for e in entries.entries:
            vd = {}
            existing[e.uuid] = (e, vd)
            for v in e.variants:
                rd = {}
                vd[v.uuid] = (v, rd)
                for r in v.revisions:
                    rd[r.uuid] = r

        for path, size, timestamp, entry_uuid in self.dirs:
            if entry_uuid in existing:
                continue
            entry = entries.entries.add()
            entry.type = {'DIR'}
            entry.relpath = path
            entry.uuid = entry_uuid
            variant = entry.variants.add()
            entry.variants.active = variant
            rev = variant.revisions.add()
            rev.size = size
            rev.timestamp = timestamp
            variant.revisions.active = rev

        if self.repo:
            for euuid, e in self.repo["entries"].items():
                entry_uuid = binascii.unhexlify(euuid)
                entry, existing_vuuids = existing.get(entry_uuid, (None, {}))
                if entry is None:
                    entry = entries.entries.add()
                    entry.uuid = entry_uuid
                    entry.name = e["name"]
                    entry.description = e["description"]
                    entry.type = {e["file_type"]}
                    entry.blender_type = e["blen_type"]
                    existing[entry_uuid] = (entry, existing_vuuids)  # Not really needed, but for sake of consistency...
                    vuuids = {}
                    self.uuids[entry.uuid] = (self.root, entry.type, entry.blender_type, vuuids)
                act_rev = None
                for vuuid, v in e["variants"].items():
                    variant_uuid = binascii.unhexlify(vuuid)
                    variant, existing_ruuids = existing_vuuids.get(variant_uuid, (None, {}))
                    if variant is None:
                        variant = entry.variants.add()
                        variant.uuid = variant_uuid
                        variant.name = v["name"]
                        variant.description = v["description"]
                        existing_vuuids[variant_uuid] = (variant, existing_ruuids)  # Not really needed, but for sake of consistency...
                        ruuids = vuuids[variant_uuid] = {}
                    if vuuid == e["variant_default"]:
                        entry.variants.active = variant
                    for ruuid, r in v["revisions"].items():
                        revision_uuid = binascii.unhexlify(ruuid)
                        revision = existing_ruuids.get(revision_uuid, None)
                        if revision is None:
                            revision = variant.revisions.add()
                            revision.uuid = revision_uuid
                            #~ revision.comment = r["comment"]
                            revision.size = r["size"]
                            revision.timestamp = r["timestamp"]
                            ruuids[revision_uuid] = (r["path_archive"], r["path"])
                        if ruuid == v["revision_default"]:
                            variant.revisions.active = revision
                            if vuuid == e["variant_default"]:
                                act_rev = r
                if act_rev:
                    entry.relpath = act_rev["path"]
            self.tags_source = sorted(self.repo["tags"].items(), key=lambda i: i[1], reverse=True)


    def status(self, job_id):
        if job_id:
            job = self.jobs.get(job_id, None)
            return job.status if job is not None else set()
        return {'VALID'}

    def progress(self, job_id):
        if job_id:
            job = self.jobs.get(job_id, None)
            return job.progress if job is not None else 0.0
        progress = 0.0
        nbr_jobs = 0
        for job in self.jobs.values():
            if 'RUNNING' in job.status:
                nbr_jobs += 1
                progress += job.progress
        return progress / nbr_jobs if nbr_jobs else 0.0

    def kill(self, job_id):
        if job_id:
            self.jobs.pop(job_id, None)
            return
        self.jobs.clear()

    def list_dir(self, job_id, entries):
        job = self.jobs.get(job_id, None)
        print(entries.root_path, job_id, job)
        if job is not None and isinstance(job, AmberJobList):
            if job.root != entries.root_path:
                self.jobs[job_id] = AmberJobList(self.executor, job_id, entries.root_path)
            else:
                job.update(self.repo, self.dirs, self.uuids)
        elif self.root != entries.root_path:
            job_id = self.job_uuid
            self.job_uuid += 1
            self.jobs[job_id] = AmberJobList(self.executor, job_id, entries.root_path)
        self.fill_entries(entries)
        return job_id

    def load_pre(self, uuids, entries):
        # Not quite sure this engine will need it in the end, but for sake of testing...
        root_path = None
        for uuid in uuids.uuids:
            root, file_type, blen_type, vuuids = self.uuids[uuid.uuid_asset]
            ruuids = vuuids[uuid.uuid_variant]
            path_archive, path = ruuids[uuid.uuid_revision]
            if root_path is None:
                root_path = root
            elif root_path != root:
                print("ERROR!!! mismatch in root paths for a same set of data, shall *never* happen (%s vs %s)" % (root_path, root))
            entry = entries.entries.add()
            entry.type = file_type
            entry.blender_type = blen_type
            # archive part not yet implemented!
            entry.relpath = path
        entries.root_path = root_path
        return True


##########
# UI stuff
class AmberPanel():
    @classmethod
    def poll(cls, context):
        space = context.space_data
        if space and space.type == 'FILE_BROWSER':
            ae = space.asset_engine
            if ae and space.asset_engine_type == "AssetEngineAmber":
                return True
        return False


class AMBER_PT_options(Panel, AmberPanel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Asset Engine"
    bl_label = "Amber Options"

    def draw(self, context):
        layout = self.layout
        space = context.space_data
        ae = space.asset_engine

        row = layout.row()
        row.prop(ae, "max_entries")


class AMBER_PT_tags(Panel, AmberPanel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Asset Engine"
    bl_label = "Tags"

    def draw(self, context):
        ae = context.space_data.asset_engine

        # Note: This is *ultra-primitive*!
        #       A good UI will most likely need new widget option anyway (template). Or maybe just some UIList...
        self.layout.props_enum(ae, "tags")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
    bpy.utils.register_class(AssetEngineFlame)
