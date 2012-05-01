#include "smoke.h"

using namespace std;
using namespace DDF;

void FLUID_3D::init()
{
	printf("-------------------- SMOKE INIT A-------------------------\n");
	{ // static scene
		SolverObject* solver = new SolverObject( "makescene", nVec3i ( _res[0], _res[1], _res[2] ), DDF_GRID_NO_FREE );
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

		_solvers.push_back(solver);
	}

	initAllSolvers();

	advanceAllSolvers();

	finalizeAllSolvers();

	_flags = _solvers[0]->getParams().getGridInt("flags");
	_normal = _solvers[0]->getParams().getGridVec3("normal");
	_dist = _solvers[0]->getParams().getGridReal("dist");

	freeAllSolvers();

	precompute();

	printf("-------------------- SMOKE INIT B-------------------------\n");
	{
		SolverObject* solver = new SolverObject( "run_static", _flags );

		Vec3 inflow (0.0, 0, 0.4); // DG TODO: get this from blender
		solver->getParams().mU0 = inflow;

		solver->getParams().mTimestepAnim = 0.005; // DG TODO: what is this parameter supposed to do?
			
		// create grids
		solver->createVec3Grid ( "mean-flow", _meanVel ); // DG TODO: load this from mem? - constructed/dumped in precompute normally
		solver->createRealGrid ( "dist", _dist );

		solver->createVec3Grid ( "vorticity", DDF_GRID_NO_FREE ); // DG TODO: how to handle vorticity?

		solver->createVec3Grid ( "ABL" ); // ABL = artificial boundary layer
		solver->createVec3Grid ( "pre-ABL", _abl ); // DG TODO: load this from mem
		solver->createVec3Grid ( "vort" ); // DG TODO: create this on init? what is this?
		solver->createRealGrid ( "pdf" ); // DG TODO: create this on init? what is this?

		solver->createRealGrid ( "density", DDF_GRID_NO_FREE ); // DG TODO: create this on init

		solver->addStandardSolverGrids(); // DG TODO: make this grids global?
		solver->createNoiseField("noise", Vec3(0.), Vec3(50,50,50), -0.4, 2.0, 0.002);

		// program solver initialization process
		// solver->addInitPlugin ( "load-universal", StringArg("grid","dist") + StringArg("file","scene/static-dist.gz"));
		// solver->addInitPlugin ( "load-universal", StringArg("grid","mean-flow") + StringArg("file","scene/static-mean.gz"));
		// solver->addInitPlugin ( "load-universal", StringArg("grid","pre-ABL") + StringArg("file","scene/static-abl.gz"));
		
		// program solver main loop
		solver->addPlugin ( "copy-grid", StringArg ( "src","mean-flow" ) + StringArg ( "dest","vel-curr") );
		solver->addPlugin ("init-density-inflow", StringArg("density","density") + RealArg("target-value",0.7) + IntArg("flag", FDENSITYSOURCE) + StringArg("noise","noise")); 
		
		solver->addPlugin ( "gen-vpart", StringArg ("source","pre-ABL") + StringArg ("flow","ABL") + StringArg("dist","dist") + StringArg("pdf","pdf") +
							RealArg("thres-vort", 2e-2) + RealArg("thres-pdf",5e-5) + RealArg("mult-pdf",1) + RealArg("scale-flow",0.94) + RealArg("max-bl",0.15) +
							RealArg("min-dist", 3) + RealArg("min-rad", 3) + RealArg("max-rad", 7) + RealArg("vortex-gain", 2.5) + RealArg("fade-in", 0));
	 
		solver->addPlugin ( "semi-lagr-advect-vec3", StringArg ( "vel-src","ABL" ) + IntArg ( "mac", 0) );
		solver->addPlugin ( "apply-vpart", StringArg ( "vorticity", "vorticity" ) );
		solver->addPlugin ( "advect-vpart");
		solver->addPlugin ( "merge-vpart", StringArg("ndist","dist") + RealArg("init-time", 30) + RealArg("decay-time",450) + RealArg("merge-dist",0.8) +
   		 					RealArg("dissipate-radius",1) + RealArg("radius-cascade",1.5) );
		solver->addPlugin ("compute-vorticity", StringArg("vorticity","vort"));	
		solver->addPlugin ("maccormack-advect-real", StringArg("real-src","density"));
		// solver->addPlugin( "dump-df3", IntArg("max-frames",200) + StringArg("gridname","density") + StringArg("prefix","sta") + RealArg("start-time",0) + IntArg("pbrt",1) + StringArg ( "override-name", "render/static" ));

		_solvers.push_back(solver);
	}

	initAllSolvers();

	_vorticity = _solvers[0]->getParams().getGridVec3("vorticity");
	_density = _solvers[0]->getParams().getGridReal("density");

}

// static precompute
void FLUID_3D::precompute(/* const std::string& name, const Vec3& inflow, int frames, bool dynamic, const Vec3& rotAxis = Vec3(0.), Real rotSpeed = 0. */)
{
	Vec3 inflow (0.0, 0, 0.4); /* DG TODO: Use blender stuff here */
	const int frames = 50; /* DG TODO: Use blender stuff here */
	SolverObject* solver = new SolverObject( "precompute", _flags );

	// create grids
	solver->createVec3Grid ( "normal", _normal );
	solver->createRealGrid ( "dist", _dist );
	solver->createVec3Grid ( "mean-vel", DDF_GRID_NO_FREE );
	solver->createVec3Grid ( "abl", DDF_GRID_NO_FREE );
	solver->addStandardSolverGrids(); // DG TODO: need to transfer them to smoke: Are these only temp grids? --> Don't think so, will get updated after every frame?

	// load grids, initialize fluid velocities
	// TODO solver->addInitPlugin ( "load-universal", StringArg("grid","dist") + StringArg("file","scene/" + name + "-dist.gz"));
	// TODO solver->addInitPlugin ( "load-universal", StringArg("grid","normal") + StringArg("file","scene/" + name + "-normal.gz"));
	solver->addInitPlugin ( "set-conditional", StringArg ( "gridname","vel-curr" ) + VecArg ( "target-vec",inflow ) + IntArg ( "flag", FFLUID ) );

	// program solver main loop
	solver->addPlugin ( "set-conditional", StringArg ( "gridname","vel-curr" ) + VecArg ( "target-vec",inflow ) + IntArg ( "flag", FINFLOW ) );
	solver->addPlugin ( "maccormack-advect-vec3", StringArg ( "vel-src", "vel-curr" ) );
	solver->addPlugin ( "set-noslip-bcs", StringArg ( "grid","vel-curr" ) );
	solver->addPlugin ( "diffuse-grid", StringArg ( "src-vec3", "vel-curr" ) + RealArg ( "diff", 0.3 ) );
	
	solver->addPlugin ( "solve-pressure", IntArg ( "openbound",0 ) );

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
	while(!advanceAllSolvers()) {};

	finalizeAllSolvers();

	_abl = _solvers[0]->getParams().getGridVec3("abl");
	_meanVel = _solvers[0]->getParams().getGridVec3("mean-vel");

	freeAllSolvers();
}

void FLUID_3D::step()
{
	// run();

	/* Calculate all requested frames */
	/* DG TODO: Can I precalc one frame at a time instead of all at once? */
	advanceAllSolvers();

}

void FLUID_3D::del()
{
	finalizeAllSolvers();
	freeAllSolvers();
}