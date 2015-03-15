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
from bpy.types import AssetEngine
from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        FloatProperty,
        EnumProperty,
        CollectionProperty,
        )

import concurrent.futures as futures
import os
import stat
import time

class AmberJob:
    def __init__(self, executor, job_id):
        self.executor = executor
        self.job_id = job_id
        self.status = {'VALID'}
        self.progress = 0.0


class AmberJobList(AmberJob):
    @staticmethod
    def ls(path):
        ret = [".."] + os.listdir(path)
        #~ time.sleep(0.1)  # 100% Artificial Lag (c)
        return ret

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

    def update(self, entries, uuids):
        if self.ls_task is not None:
            if not self.ls_task.done():
                return
            paths = self.ls_task.result()
            self.ls_task = None
            self.tot = len(paths)
            for p in paths:
                self.stat_tasks.add(self.executor.submit(self.stat, self.root, p))
        else:
            done = set()
            for tsk in self.stat_tasks:
                if tsk.done():
                    path, (is_dir, size, timestamp) = tsk.result()
                    self.nbr += 1

                    if is_dir:
                        # We only list dirs from real file system.
                        entry = entries.entries.add()
                        entry.type = {'DIR'}
                        entry.relpath = path
                        entry.uuid = entry.relpath.encode()[:8] + b"|" + bytes(self.nbr)
                        uuids[entry.uuid] = self.root + path
                        variant = entry.variants.add()
                        entry.variants.active = variant
                        rev = variant.revisions.add()
                        rev.size = size
                        rev.timestamp = timestamp
                        variant.revisions.active = rev

                    done.add(tsk)
            self.stat_tasks -= done
        self.progress = self.nbr / self.tot
        if not self.stat_tasks and self.ls_task is None:
            self.status = {'VALID'}

    def __init__(self, executor, job_id, root):
        super().__init__(executor, job_id)
        self.root = root
        self.entries = {}
        self.ls_task = None
        self.stat_tasks = set()

        self.start()

    def __del__(self):
        # Avoid useless work!
        if self.ls_task is not None:
            self.ls_task.cancel()
        for tsk in self.stat_tasks:
            tsk.cancel()


class AssetEngineAmber(AssetEngine):
    bl_label = "Amber"

    def __init__(self):
        self.executor = futures.ThreadPoolExecutor(8)  # Using threads for now, if issues arise we'll switch to process.
        self.jobs = {}
        self.uuids = {}

        self.job_uuid = 1

    def __del__(self):
        self.executor.shutdown(wait=False)

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
        for job in self.jobs.values():
            job.kill()

    def list_dir(self, job_id, entries):
        job = self.jobs.get(job_id, None)
        print(entries.root_path, job_id, job)
        if job is not None and isinstance(job, AmberJobList):
            if job.root != entries.root_path:
                self.jobs[job_id] = AmberJobList(self.executor, job_id, entries.root_path)
            else:
                job.update(entries, self.uuids)
        else:
            self.job_uuid += 1
            job_id = self.job_uuid
            self.jobs[job_id] = AmberJobList(self.executor, job_id, entries.root_path)
        return job_id

    def load_pre(self, uuids, entries):
        # Not quite sure this engine will need it in the end, but for sake of testing...
        entries.root_path = "/"
        for uuid in uuids.uuids[:1]:
            entry = entries.entries.add()
            entry.type = {'BLENDER'}
            entry.relpath = self.uuids[uuid.uuid_asset]
        return True


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
    bpy.utils.register_class(AssetEngineFlame)
