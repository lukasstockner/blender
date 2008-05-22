#!BPY

"""
Name: 'NLA Prototype...'
Blender: 246
Group: 'Animation'
Tip: 'Interactivly prototype the NLA'
"""

__author__ = "Campbell Barton"
__url__ = ("blender.org", "blenderartists.org")
__version__ = "0.01 05/22/2008"

__bpydoc__ = """\
"""

# -------------------------------------------------------------------------- 
# Anim Prototype v0.01 by Campbell Barton (AKA Ideasman42) 
# -------------------------------------------------------------------------- 
# ***** BEGIN GPL LICENSE BLOCK ***** 
# 
# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation; either version 2 
# of the License, or (at your option) any later version. 
# 
# This program is distributed in the hope that it will be useful, 
# but WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the 
# GNU General Public License for more details. 
# 
# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software Foundation, 
# Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA. 
# 
# ***** END GPL LICENCE BLOCK ***** 
# -------------------------------------------------------------------------- 

import bpy
import Blender
from Blender import Window, Draw

FLAG_IS_STATE = 1<<0 # Is this a switch state
FLAG_IS_LOOP = 1<<1 # Is this a switch state


IDLE = 'Frankie_Idle1'

KMKEY = 0
KMNAME = 1
KMFLAG = 2
KMSTATE = 3
KMPRIORITY = 4

# key, actionName, flags, pressed?
KEYMAP = [\
[Draw.UPARROWKEY,		'Frankie_Walk',			FLAG_IS_STATE|FLAG_IS_LOOP,		False, 10],\
[Draw.LEFTARROWKEY,		IDLE,					FLAG_IS_STATE|FLAG_IS_LOOP,		False, 0],\
[Draw.RIGHTARROWKEY,	IDLE,					FLAG_IS_STATE|FLAG_IS_LOOP,		False, 0],\
[Draw.DOWNARROWKEY,		'Frankie_Runs',			FLAG_IS_STATE|FLAG_IS_LOOP,		False, 10],\
[Draw.SPACEKEY,			'Frankie_StandJump',	FLAG_IS_STATE,					False, 20],\
[Draw.RETKEY,			'Frankie_Throw1',		FLAG_IS_STATE,					False, 10],\
]

ESC = Draw.ESCKEY

def recalcRepeat(s):
	actlen= s.actionEnd-s.actionStart
	s.repeat = ((s.stripEnd-s.stripStart) / actlen)
	

def setStripStart(s, frame):
	stripStart =	s.stripStart
	stripEnd =		s.stripEnd

	if frame == stripStart:
		return

	stripLen = 		stripEnd - stripStart
	
	if frame < stripStart:
		s.stripStart =	frame
		s.stripEnd =	frame + stripLen
	else:
		s.stripEnd =	frame + stripLen
		s.stripStart =	frame

def setStripEnd(s, frame):
	stripStart =	s.stripStart
	stripEnd =		s.stripEnd

	if frame == stripEnd:
		return
	stripLen = 		stripEnd - stripStart
	
	if frame < stripEnd:
		s.stripStart =	frame-stripLen
		s.stripEnd =	frame
	else:
		s.stripEnd =	frame
		s.stripStart =	frame-stripLen

def setStripInf(s):
	s.stripEnd = s.stripStart + 1000
	recalcRepeat(s)

def setStripOnce(s):
	actlen= s.actionEnd-s.actionStart
	s.stripEnd = s.stripStart + actlen
	recalcRepeat(s)

def fadeIn(s, frame):
	#setStripStart(s, frame)
	setStripStart(s, frame)
	setStripInf(s)
	
def fadeOut(s, frame):
	'''
	length = s.actionEnd - s.actionStart 
	setStripEnd(s, frame + int(s.blendOut))
	'''
	
	if s.stripStart > frame:
		return 
	
	
	stripStartNew = s.stripStart
	stripLenNew = 	frame - stripStartNew
	actlen =		s.actionEnd-s.actionStart
	
	while stripLenNew - (s.blendIn+s.blendOut) < 0:
		stripStartNew -= actlen
		stripLenNew = 	frame - stripStartNew
		#print "make way!", s.action.name
		#print frame,stripLenNew 
	
	s.stripStart = stripStartNew
	s.stripEnd = frame
	
	recalcRepeat(s)
	
	#s.stripEnd =	frame - s.blendOut
	#recalcRepeat(s)
	
	#setStripEnd(s, frame)

def animloop(sce, rend, ob):
	
	jump_momentum = None
	
	def IS_ACTIVE_STATE(km_context):
		for km in KEYMAP:
			if km_context != km and km[KMSTATE] and km[KMFLAG] & FLAG_IS_STATE:
				return True
		return False
	
	
	

	cfra = rend.cFrame


	
	strips = []
	
	
	def BUILD_STRIPS():
		strips[:] = [(s,s.action) for s in ob.actionStrips]
	
	BUILD_STRIPS()
	
	def GET_PRIORITY(flag):
		priority = -1
		for km in KEYMAP:
			if km[KMSTATE]: # Make sure this wasnt set
				if km[KMFLAG] & flag:
					priority = max(priority, km[KMPRIORITY])
		return priority
	
	
	def TEST_STRIP(frame):
		for s,a in strips:
			if not (s.flag & Blender.Armature.NLA.Flags.MUTE):
				if s.stripStart+s.blendIn <= frame and s.stripEnd-s.blendOut >= frame:
					# print s.action.name
					return True
		# print "NBOPATHC"
		return False
		
		
	def SET_ACTIVE(km):
		blend = 0
		ok = False
		
		for s,a in strips:
			if a.name == km[KMNAME]:
				ok = True
				break
		
		if ok:			
			# blend = s.blendOut
			
			fadeIn(s, cfra)
			s.flag &= ~Blender.Armature.NLA.Flags.MUTE
			
			if not (km[KMFLAG] & FLAG_IS_LOOP):
				setStripOnce(s)
			
			for i in xrange(len(strips)):
				ob.actionStrips.moveUp(s)
			BUILD_STRIPS()
				
		
		for s,a in strips:
			if a.name != km[KMNAME]:
				fadeOut(s, cfra + s.blendOut)
		
	
	
	def SET_IDLE():
		blend = 0
		ok = False
		for s,a in strips:
			# print a.name
			if a.name == IDLE:
				# setStripStart(s, cfra)
				ok = True
				break
		if ok:		
			blend = s.blendOut
			fadeIn(s, cfra)
			s.flag &= ~Blender.Armature.NLA.Flags.MUTE
			
			for i in xrange(len(strips)):
				ob.actionStrips.moveUp(s)
			BUILD_STRIPS()
		
		for s,a in strips:
			if a.name != IDLE:
				fadeOut(s, cfra+s.blendOut)
	
	for s,a in strips:
		# print a.name
		if a.name == IDLE:
			setStripStart(s, cfra)
			setStripInf(s)
			s.flag &= ~Blender.Armature.NLA.Flags.MUTE
		else:
			setStripOnce(s)
			setStripEnd(s, 0)
			s.flag |= Blender.Armature.NLA.Flags.MUTE
	
	context_act = IDLE
	
	while 1:
		cfra = rend.cFrame
		# sce.update()
		#print "cfra", cfra
		while Window.QTest():
			evt, val = Window.QRead()
			if evt == Draw.MOUSEX or evt == Draw.MOUSEY:
				pass
			else:
				print evt, val
				
				priority = GET_PRIORITY(FLAG_IS_STATE)
				
				for km in KEYMAP:
					if km[0] == evt:
						if val:
							if not km[KMSTATE]: # Make sure this wasnt set
								if km[KMFLAG] & FLAG_IS_STATE and priority <= km[KMPRIORITY]:
									if context_act != km[KMNAME]:
										context_act = km[KMNAME]
										SET_ACTIVE(km)
											
							km[KMSTATE] = val
						else:
							if km[KMFLAG] & FLAG_IS_STATE and (km[KMFLAG] & FLAG_IS_LOOP):
								if not IS_ACTIVE_STATE(km):
									if context_act != IDLE:
										# print "SETTING IDLE1"
										context_act = IDLE
										SET_IDLE()
										context_act = IDLE
				
								km[KMSTATE] = val
							
							# In some cases we want to keep km[KMSTATE] even if the key is off
					
			if evt == ESC:
				return	

		for s,a in strips:
			if s.stripEnd < cfra:
				s.flag |= Blender.Armature.NLA.Flags.MUTE

		# End all loops that are active but have finished
		for km in KEYMAP:
			if km[KMSTATE] and not (km[KMFLAG] & FLAG_IS_LOOP):
				for s,a in strips:
					if a.name == km[KMNAME]:
						if s.stripEnd - s.blendOut <= cfra:
							# print s.stripEnd, cfra
							km[KMSTATE] = False
							# print "TEST!!!"
							
		
		# Set Idle if none are active
		if not IS_ACTIVE_STATE(None):
			if context_act != IDLE:
				context_act = IDLE
				SET_IDLE()
				context_act = IDLE
		else:
			priority = GET_PRIORITY(FLAG_IS_STATE | FLAG_IS_LOOP)
			if not TEST_STRIP(cfra):
				for km in KEYMAP:
					if km[KMSTATE]:
						if km[KMFLAG] & FLAG_IS_STATE and km[KMFLAG] & FLAG_IS_LOOP and priority <= km[KMPRIORITY]:
							context_act = km[KMNAME]
							SET_ACTIVE(km)
			
		
		for km in KEYMAP:			
			if km[KMSTATE]: # This key is held
				if km[KMKEY] == Draw.LEFTARROWKEY:
					ob.RotZ-=0.1
				elif km[KMKEY] == Draw.RIGHTARROWKEY:
					ob.RotZ+=0.1
				elif km[KMKEY] == Draw.UPARROWKEY or km[KMKEY] == Draw.DOWNARROWKEY:
					mat = ob.matrixWorld.rotationPart()
					if km[KMKEY] == Draw.DOWNARROWKEY:
						ofs = Blender.Mathutils.Vector(0,0.04,0) * mat
					else:
						ofs = Blender.Mathutils.Vector(0,0.02,0) * mat					
					ob.LocX += ofs.x
					ob.LocY += ofs.y
					#ob.LocZ += ofs.z
				elif km[KMKEY] == Draw.SPACEKEY:
					if jump_momentum == None:
						jump_momentum = 0.15
					
					
		if jump_momentum != None:
			ob.LocZ += jump_momentum
		
			if ob.LocZ < 0:
				ob.LocZ = 0
				jump_momentum = None
				
				# Force the key off
				for km in KEYMAP:
					if km[KMKEY] == Draw.SPACEKEY:
						km[KMSTATE] = False
			else:
				jump_momentum -= 0.01
				
					
				
				
		
			
		rend.cFrame = cfra+1			
		
		ob.makeDisplayList()
		
		Window.RedrawAll()
					




def main():
	sce = bpy.data.scenes.active
	rend = sce.render
	ob = sce.objects.active
	
	back_cfra = rend.cFrame
	back_sfra = rend.sFrame
	back_efra = rend.eFrame
	
	rend.sFrame = 1
	rend.eFrame = 300000
	
	
	if not ob:
		print 'No Active Object'
		return
	
	animloop(sce, rend, ob)
	
	rend.cFrame = back_cfra
	rend.sFrame = back_sfra
	rend.eFrame = back_efra
	
	
	Window.RedrawAll()


'''
def debug():
	sce = bpy.data.scenes.active
	rend = sce.render
	ob = sce.objects.active
	for s in ob.actionStrips:
		# print s.action.name
		pass
	
	
	
	#recalcRepeat(s)
	#setStripEnd(s, 100)
	#setStripEnd(s, 100)
	#setStripEnd(s, 200)
	#setStripEnd(s, 100)
	#setStripEnd(s, 100)
	
	Window.RedrawAll()
	ob.makeDisplayList()
'''




if __name__ == '__main__':
	main()
	#debug()



