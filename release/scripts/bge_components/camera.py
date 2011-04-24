import bge
from collections import OrderedDict

class ThirdPerson(bge.types.KX_PythonComponent):
	"""A component for a third person camera"""
	
	args = OrderedDict([
		("pPivot Name", "pivot"),
		("Time Offset", 35),
		("Lens", 30),
		("Scale Max", 4.0),
		("Scale Min", 0.6),
		("Option", {"One", "Two"})
		])
		
	def start(self, args):
		# Make sure we have a camera
		if not isinstance(self.object, bge.types.KX_Camera):
			raise TypeError("This component must be attached to a camera")
		
		print(args['Option'])
		# Apply settings
		self.object.parent.timeOffset = args['Time Offset']
		self.object.lens = args['Lens']
		
		# Save scale settings
		self.scale_max = args['Scale Max']
		self.scale_min = args['Scale Min']
		
		# Find the pivot
		pivot = self.object.parent
		
		while pivot:
			if pivot.name == args['Pivot Name']:
				break

			pivot = pivot.parent
			
		if not pivot:
			raise ValueError("Could not find the pivot object")
			
		self.pivot = pivot
		
		self._target_distance = (self.object.worldPosition - pivot.worldPosition).length
		
	def update(self):
		ob = self.object
		pivot = self.pivot
	
		# Cast a ray and see if we hit something
		hit_pos = self.object.rayCast(
					ob.worldPosition,
					pivot.worldPosition,
					ob.localPosition[2])[1]
					
		if hit_pos:
			scale = ob.getDistanceTo(hit_pos)/self._target_distance
			if scale > self.scale_max:
				scale = self.scale_max
			elif scale < self.scale_min:
				scale = self.scale_min
		else:
			scale = self.scale_max
			
		# Apply the scaling
		pivot.scaling = [scale, scale, scale]
		
		# Undo the scaling on the camera
		# inv_scale = 1/scale
		# ob.scaling = [inv_scale, inv_scale, inv_scale]
		
		# Update the "look at"
		vec = ob.getVectTo(pivot)[1]
		ob.alignAxisToVect(vec, 1)