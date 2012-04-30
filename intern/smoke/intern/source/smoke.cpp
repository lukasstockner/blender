#include "smoke.h"

using namespace std;
using namespace DDF;

void FLUID_3D::init()
{
	{ // static scene
		SolverObject* solver = new SolverObject( "makescene", nVec3i ( _res[0], _res[1], _res[2] ) );
		solver->createVec3Grid ( "normal", DDF_GRID_NO_FREE /* Do not free grid when freeing solver */);
		solver->createRealGrid ( "dist", DDF_GRID_NO_FREE );
		solver->addInitPlugin ( "init-box-domain",  IntArg ( "flag-inside",FFLUID ) + IntArg ( "flag-border",FINFLOW ) + IntArg("flag-floor", FOBSTACLE) );
		
		// obstacles
		solver->addInitPlugin ( "init-sphere", VecArg("center",Vec3(0.25,0.6,0.35)) + RealArg("radius",0.1) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.2,0.0,0.3)) + VecArg("pos2",Vec3(0.3,0.6,0.4)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.3,0.0,0.65)) + VecArg("pos2",Vec3(0.5,0.3,0.9)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.6,0.0,0.3)) + VecArg("pos2",Vec3(0.65,0.4,0.5)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		
		// smoke seed
		solver->addInitPlugin ( "init-sphere", VecArg("center",Vec3(0.3,0.65,0.35)) + RealArg("radius",0.1) + IntArg("type", FDENSITYSOURCE));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.25,0.0,0.3)) + VecArg("pos2",Vec3(0.35,0.65,0.4)) + IntArg("type", FDENSITYSOURCE));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.35,0.0,0.65)) + VecArg("pos2",Vec3(0.55,0.35,0.9)) + IntArg("type", FDENSITYSOURCE));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.65,0.0,0.3)) + VecArg("pos2",Vec3(0.7,0.45,0.5)) + IntArg("type", FDENSITYSOURCE));	

		// solver->addInitPlugin ( "dump-universal", StringArg ( "grid", "flags" ) + StringArg ( "override-name","scene/static-flags" )  + IntArg ( "single-dump", 1 ) );	
		// solver->addInitPlugin ( "dump-universal", StringArg ( "grid", "dist" ) + StringArg ( "override-name","scene/static-dist" )  + IntArg ( "single-dump", 1 ) );	
		// solver->addInitPlugin ( "dump-universal", StringArg ( "grid", "normal" ) + StringArg ( "override-name","scene/static-normal" )  + IntArg ( "single-dump", 1 ) );	
		_solvers.push_back(solver);
	}

	initAllSolvers();

	advanceAllSolvers();

	_flags = _solvers[0]->getParams().getGridInt("flags");
	_normal = _solvers[0]->getParams().getGridVec3("normal");
	_dist = _solvers[0]->getParams().getGridReal("dist");

	finalizeAllSolvers();
}

// static precompute
void FLUID_3D::precompute(/* const std::string& name, const Vec3& inflow, int frames, bool dynamic, const Vec3& rotAxis = Vec3(0.), Real rotSpeed = 0. */)
{
	Vec3 inflow (0.4, 0, 0); /* DG TODO: Use blender stuff here */
	const int frames = 50; /* DG TODO: Use blender stuff here */
	// bool rotate = (rotSpeed != 0.);
	SolverObject* solver = new SolverObject( "precompute", _flags );

	// create grids
	solver->createVec3Grid ( "normal", _normal );
	solver->createRealGrid ( "dist", _dist );
	solver->createVec3Grid ( "mean-vel", DDF_GRID_NO_FREE );
	solver->createVec3Grid ( "abl", DDF_GRID_NO_FREE );
	solver->addStandardSolverGrids(); // DG TODO: need to transfer them to smoke: Are these only temp grids? --> Don't think so, will get updated after every frame?
	
	// additional grids for rot. precomputation
	/*
	if (rotate) {
		solver->createIntGrid ("obstacle-flags");
		solver->createIntGrid ("empty-flags");
		solver->addInitPlugin ( "load-universal", StringArg("grid","obstacle-flags") + StringArg("file","scene/" + name + "-flags.gz"));
		solver->addInitPlugin ( "init-box-domain",  StringArg("gridname","empty-flags") + IntArg ( "flag-inside",FFLUID ) + IntArg ( "flag-border",FINFLOW ) );		
	}
	*/

	// load grids, initialize fluid velocities
	// TODO solver->addInitPlugin ( "load-universal", StringArg("grid","dist") + StringArg("file","scene/" + name + "-dist.gz"));
	// TODO solver->addInitPlugin ( "load-universal", StringArg("grid","normal") + StringArg("file","scene/" + name + "-normal.gz"));
	solver->addInitPlugin ( "set-conditional", StringArg ( "gridname","vel-curr" ) + VecArg ( "target-vec",inflow ) + IntArg ( "flag", FFLUID ) );

	// program solver main loop
	solver->addPlugin ( "set-conditional", StringArg ( "gridname","vel-curr" ) + VecArg ( "target-vec",inflow ) + IntArg ( "flag", FINFLOW ) );
	solver->addPlugin ( "maccormack-advect-vec3", StringArg ( "vel-src", "vel-curr" ) );
	solver->addPlugin ( "set-noslip-bcs", StringArg ( "grid","vel-curr" ) );
	solver->addPlugin ( "diffuse-grid", StringArg ( "src-vec3", "vel-curr" ) + RealArg ( "diff", 0.3 ) );
	// if (rotate)
	//	solver->addPlugin ("set-moving-obs-bcs", StringArg("obstacle","obstacle-flags") + StringArg("flags-src","empty-flags") +
	//						VecArg("obs-rot-axis", rotAxis) + RealArg("obs-rot-vel", rotSpeed) + VecArg("obs-center", Vec3(0.5,0.5,0.5)));

	solver->addPlugin ( "solve-pressure", IntArg ( "openbound",0 ) );
	
	// if (rotate)
	//	solver->addPlugin ( "average", StringArg ( "gridname","vel-curr" ) + StringArg ( "sumgrid","mean-vel" ) + IntArg ( "from", frames ) + IntArg ( "frames", 3 ) + IntArg ( "post-quit",1 ) + IntArg("stride", frames) );
	// else
		solver->addPlugin ( "average", StringArg ( "gridname","vel-curr" ) + StringArg ( "sumgrid","mean-vel" ) + IntArg ( "from", frames ) + IntArg ( "frames", frames ) + IntArg ( "post-quit",1 ) );
	
	// program final steps
	solver->addEndPlugin ( "calc-abl", StringArg ("mean-vel","mean-vel") + StringArg ("dist","dist") + StringArg("normal","normal") + StringArg("abl","abl") + RealArg("d", 1.7));
	
	// if (dynamic)
	// 	solver->addEndPlugin ( "add-database", StringArg("grid","abl") + StringArg("normal","normal") + VecArg("u0",rotate ? (rotSpeed*rotAxis):inflow));
	// else 
	// {
	//	solver->addEndPlugin ( "dump-universal", StringArg ( "grid","abl" ) + StringArg ( "override-name","scene/" + name + "-abl" )  + IntArg ( "single-dump", 1 ) );	
	//	solver->addEndPlugin ( "dump-universal", StringArg ( "grid","mean-vel" ) + StringArg ( "override-name","scene/" + name + "-mean" ) + IntArg ( "single-dump", 1 ) );
	// }
		
	_solvers.push_back(solver);		
	
	// run();

	initAllSolvers();

	/* Calculate all requested frames */
	/* DG TODO: Can I precalc one frame at a time instead of all at once? */
	while(advanceAllSolvers()) {};

	_abl = _solvers[0]->getParams().getGridVec3("abl");
	_meanVel = _solvers[0]->getParams().getGridVec3("mean-vel");

	finalizeAllSolvers();
}

void FLUID_3D::step()
{
}