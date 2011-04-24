import bge

class ThirdPerson(bge.types.KX_PythonComponent):
	"""Basic third person controls
	
	W: move forward
	A: turn left
	S: move backward
	D: turn right
	
	"""
	
	args = {
		"Move Speed": 10,
		"Turn Speed": 0.04
	}
	
	def start(self, args):
		self.move_speed = args['Move Speed']
		self.turn_speed = args['Turn Speed']
		
	def update(self):
		keyboard = bge.logic.keyboard.events
		
		move = 0
		rotate = 0
		
		if keyboard[bge.events.WKEY]:
			move += self.move_speed
		if keyboard[bge.events.SKEY]:
			move -= self.move_speed
			
		if keyboard[bge.events.AKEY]:
			rotate += self.turn_speed
		if keyboard[bge.events.DKEY]:
			rotate -= self.turn_speed
			
		self.object.setLinearVelocity((0, move, 0), True)
		self.object.applyRotation((0, 0, rotate), True)
