import bge
from collections import OrderedDict

class Vehicle(bge.types.KX_PythonComponent):
	"""A component making use of the vehicle wrapper
	
	Controls:
	W: Move forward
	S: Move backward
	A: Turn left
	D: Turn right
	SPACE: Brake
	
	"""
	
	args = OrderedDict([
		("Gas Power", 15.0),
		("Reverse Power", 10.0),
		("Brake Power", 10.0),
		("Turn Power", 0.3),
		
		("Front Wheel Drive", True),
		("Rear Wheel Drive", False),
		("Tire Prefix", "tire_"),
		("Front Tire Radius", 0.3),
		("Rear Tire Radius", 0.3),
		("Tire Friction", 10.0),
		("Suspension Height", 0.2),
		("Suspension Compression", 6.0),
		("Suspension Damping", 1.0),
		("Suspension Stiffness", 20.0),
		("Roll Influence", 0.06),
		])
	
	def start(self, args):
	
		# Save power settings
		self.gas = args['Gas Power']
		self.reverse = args['Reverse Power']
		self.brake = args['Brake Power']
		self.turn = args['Turn Power']
		
		# Save steering settings
		self.fwd = args['Front Wheel Drive']
		self.rwd = args['Rear Wheel Drive']
		
		# Create the vehicle constraint
		constraint = bge.constraints.createConstraint(self.object.getPhysicsId(), 0, 11)
		cid = constraint.getConstraintId()
		vid = bge.constraints.getVehicleConstraint(cid)
		self.vid = vid
		
		# Find the tires (they should be parented)
		tpx = args['Tire Prefix']
		tires = [None]*4
		for child in self.object.childrenRecursive:
			for i in range(4):
				if child.name.startswith(tpx+str(i+1)):
					tires[i] = child
					
					# Unparent the tire so it doesn't cause the vehicle wrapper grief
					child.removeParent()

		# Verify that we have all of the tires
		for idx, tire in enumerate(tires):
			if tire is None:
				raise ValueError("Tire "+str(idx+1)+" not found")
				
		# Now setup the tires
		for i in range(4):			
			# Add the wheel
			vid.addWheel(
					# Object
					tires[i],
					
					# Position
					tires[i].worldPosition - self.object.worldPosition,
					
					# Suspension angle
					(0, 0, -1),
					
					# Suspension axis
					(-1, 0, 0),
					
					# Suspension height
					args['Suspension Height'],
					
					# Tire radius
					args['Front Tire Radius'] if i in (0, 1) else args['Rear Tire Radius'],
					
					# Steerability
					args['Front Wheel Drive'] if i in (2, 3) else args['Rear Wheel Drive'])
					
			# Advanced settings
			vid.setTyreFriction(args['Tire Friction'], i)
			vid.setSuspensionCompression(args['Suspension Compression'], i)
			vid.setSuspensionDamping(args['Suspension Damping'], i)
			vid.setSuspensionStiffness(args['Suspension Stiffness'], i)
			vid.setRollInfluence(args['Roll Influence'], i)
					
	def update(self):
		keyboard = bge.logic.keyboard.events
		# Engine force
		engine_force = 0
		if keyboard[bge.events.WKEY] == bge.logic.KX_INPUT_ACTIVE:
			engine_force -= self.gas
		if keyboard[bge.events.SKEY] == bge.logic.KX_INPUT_ACTIVE:
			engine_force += self.gas
			
		# Steering
		steering = 0
		if keyboard[bge.events.AKEY] == bge.logic.KX_INPUT_ACTIVE:
			steering += self.turn
		if keyboard[bge.events.DKEY] == bge.logic.KX_INPUT_ACTIVE:
			steering -= self.turn
			
		# Braking
		braking = 0
		if keyboard[bge.events.SPACEKEY] == bge.logic.KX_INPUT_ACTIVE:
			braking += self.brake
			
		# Apply settings
		
		for i in range(4):
			self.vid.applyEngineForce(engine_force, i)
			
			if (i in (0, 1) and self.fwd) or (i in (2, 3) and self.rwd):
				self.vid.applyBraking(braking, i)
				self.vid.setSteeringValue(steering, i)
