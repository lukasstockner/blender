#include "smoke.h"

using namespace std;
using namespace DDF;

#include <limits.h>

void FLUID_3D::init()
{
	printf("-------------------- SMOKE INIT A-------------------------\n");
	{ // static scene
		printf("init res %d, %d, %d\n", _res[0], _res[1], _res[2]);
		SolverObject* solverInit = new SolverObject( "makescene", nVec3i ( _res[0], _res[1], _res[2] ), DDF_GRID_NO_FREE ); // 200, 80, 150
		solverInit->createVec3Grid ( "normal", DDF_GRID_NO_FREE );
		solverInit->createRealGrid ( "dist", DDF_GRID_NO_FREE );
		solverInit->addInitPlugin ( "init-box-domain",  IntArg ( "flag-inside",FFLUID ) + IntArg ( "flag-border",FINFLOW ) + IntArg("flag-floor", FOBSTACLE) );
		// obstacles
		solverInit->addInitPlugin ( "init-sphere", VecArg("center",Vec3(0.25,0.6,0.35)) + RealArg("radius",0.1) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solverInit->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.2,0.0,0.3)) + VecArg("pos2",Vec3(0.3,0.6,0.4)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solverInit->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.3,0.0,0.65)) + VecArg("pos2",Vec3(0.5,0.3,0.9)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solverInit->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.6,0.0,0.3)) + VecArg("pos2",Vec3(0.65,0.4,0.5)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		// smoke seed
		solverInit->addInitPlugin ( "init-sphere", VecArg("center",Vec3(0.3,0.65,0.35)) + RealArg("radius",0.1) + IntArg("type", FDENSITYSOURCE));	
		solverInit->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.25,0.0,0.3)) + VecArg("pos2",Vec3(0.35,0.65,0.4)) + IntArg("type", FDENSITYSOURCE));	
		solverInit->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.35,0.0,0.65)) + VecArg("pos2",Vec3(0.55,0.35,0.9)) + IntArg("type", FDENSITYSOURCE));	
		solverInit->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.65,0.0,0.3)) + VecArg("pos2",Vec3(0.7,0.45,0.5)) + IntArg("type", FDENSITYSOURCE));	

		// solverInit->addInitPlugin ( "dump-universal", StringArg ( "grid", "flags" ) + StringArg ( "override-name","scene/static-flags" )  + IntArg ( "single-dump", 1 ) );	
		// solverInit->addInitPlugin ( "dump-universal", StringArg ( "grid", "dist" ) + StringArg ( "override-name","scene/static-dist" )  + IntArg ( "single-dump", 1 ) );	
		// solverInit->addInitPlugin ( "dump-universal", StringArg ( "grid", "normal" ) + StringArg ( "override-name","scene/static-normal" )  + IntArg ( "single-dump", 1 ) );

		_solvers.push_back(solverInit);
	}

	initAllSolvers();
	
	// advanceAllSolvers(); // DG TODO disabled for testing purposed

	// finalizeAllSolvers(); // DG TODO disabled for testing purposed

	_flags = _solvers[0]->getParams().getGridInt("flags");
	_normal = _solvers[0]->getParams().getGridVec3("normal");
	_dist = _solvers[0]->getParams().getGridReal("dist");

	freeAllSolvers();
/*
	{
		Vec3 inflow (0.4, 0, 0);
		const int nFrames = 50;
		precompute("static", inflow, nFrames, false);
	}
*/
	printf("-------------------- SMOKE INIT B-------------------------\n");
	{
		Vec3 inflow (0.4, 0, 0);
		SolverObject* solverStep = new SolverObject( "run_static",    _flags /* "scene/static-flags.gz" */ );

		solverStep->getParams().mU0 = inflow;
		solverStep->getParams().mTimestepAnim = 0.005;
			
		// create grids
		solverStep->createVec3Grid ( "mean-flow" );
		solverStep->createRealGrid ( "dist", _dist );
		solverStep->createVec3Grid ( "vorticity", DDF_GRID_NO_FREE );
		solverStep->createVec3Grid ( "ABL" );
		solverStep->createVec3Grid ( "pre-ABL" );
		solverStep->createVec3Grid ( "vort" );
		solverStep->createRealGrid ( "pdf" );
		solverStep->createRealGrid ( "density", DDF_GRID_NO_FREE );
		solverStep->addStandardSolverGrids();
		solverStep->createNoiseField("noise", Vec3(0.), Vec3(50,50,50), -0.4, 20.0 /* 2.0 */, 0.002);

		// program solverStep initialization process
		// solverStep->addInitPlugin ( "load-universal", StringArg("grid","dist") + StringArg("file","scene/static-dist.gz"));
		solverStep->addInitPlugin ( "load-universal", StringArg("grid","mean-flow") + StringArg("file","scene/static-mean.gz"));
		solverStep->addInitPlugin ( "load-universal", StringArg("grid","pre-ABL") + StringArg("file","scene/static-abl.gz"));
		
		// program solver main loop
		solverStep->addPlugin ( "copy-grid", StringArg ( "src","mean-flow" ) + StringArg ( "dest","vel-curr") );
		solverStep->addPlugin ("init-density-inflow", StringArg("density","density") + RealArg("target-value",0.7) + IntArg("flag", FDENSITYSOURCE) + StringArg("noise","noise"));
		solverStep->addPlugin ( "gen-vpart", StringArg ("source","pre-ABL") + StringArg ("flow","ABL") + StringArg("dist","dist") + StringArg("pdf","pdf") +
							RealArg("thres-vort", 2e-2) + RealArg("thres-pdf",5e-5) + RealArg("mult-pdf",1) + RealArg("scale-flow", 0.94) + RealArg("max-bl",0.15) +
							RealArg("min-dist", 3) + RealArg("min-rad", 3) + RealArg("max-rad", 7) + RealArg("vortex-gain", 2.5) + RealArg("fade-in", 0));
	 
		solverStep->addPlugin ( "semi-lagr-advect-vec3", StringArg ( "vel-src","ABL" ) + IntArg ( "mac", 0) );
		solverStep->addPlugin ( "apply-vpart", StringArg ( "vorticity", "vorticity" ) );
		solverStep->addPlugin ( "advect-vpart");
		solverStep->addPlugin ( "merge-vpart", StringArg("ndist","dist") + RealArg("init-time", 30) + RealArg("decay-time",450) + RealArg("merge-dist",0.8) +
   							RealArg("dissipate-radius",1) + RealArg("radius-cascade",1.5) );
		solverStep->addPlugin ("compute-vorticity", StringArg("vorticity","vort"));	
		solverStep->addPlugin ("maccormack-advect-real", StringArg("real-src","density"));
		// solverStep->addPlugin( "dump-df3", IntArg("max-frames",200) + StringArg("gridname","density") + StringArg("prefix","sta") + RealArg("start-time",0) + IntArg("pbrt",1) + StringArg ( "override-name", "render/static" ));

		_solvers.push_back(solverStep);
	}

	initAllSolvers();

	_vorticity = _solvers[0]->getParams().getGridVec3("vorticity");
	_density = _solvers[0]->getParams().getGridReal("density");

	_init = 1;

}

// static precompute
void FLUID_3D::precompute(const std::string& name, const Vec3& inflow, int frames, bool dynamic, const Vec3& rotAxis, Real rotSpeed)
{
	bool rotate = (rotSpeed != 0.);
	SolverObject* solverPreCalc = new SolverObject( "precompute", _flags );

	// create grids
	solverPreCalc->createVec3Grid ( "normal" );
	solverPreCalc->createRealGrid ( "dist" );
	solverPreCalc->createVec3Grid ( "mean-vel" );
	solverPreCalc->createVec3Grid ( "abl" );
	solverPreCalc->addStandardSolverGrids();
	
	// additional grids for rot. precomputation
	if (rotate) {
		solverPreCalc->createIntGrid ("obstacle-flags");
		solverPreCalc->createIntGrid ("empty-flags");
		solverPreCalc->addInitPlugin ( "load-universal", StringArg("grid","obstacle-flags") + StringArg("file","scene/" + name + "-flags.gz"));
		solverPreCalc->addInitPlugin ( "init-box-domain",  StringArg("gridname","empty-flags") + IntArg ( "flag-inside",FFLUID ) + IntArg ( "flag-border",FINFLOW ) );		
	}

	// load grids, initialize fluid velocities
	solverPreCalc->addInitPlugin ( "load-universal", StringArg("grid","dist") + StringArg("file","scene/" + name + "-dist.gz"));
	solverPreCalc->addInitPlugin ( "load-universal", StringArg("grid","normal") + StringArg("file","scene/" + name + "-normal.gz"));
	solverPreCalc->addInitPlugin ( "set-conditional", StringArg ( "gridname","vel-curr" ) + VecArg ( "target-vec",inflow ) + IntArg ( "flag", FFLUID ) );

	// program solver main loop
	solverPreCalc->addPlugin ( "set-conditional", StringArg ( "gridname","vel-curr" ) + VecArg ( "target-vec",inflow ) + IntArg ( "flag", FINFLOW ) );
	solverPreCalc->addPlugin ( "maccormack-advect-vec3", StringArg ( "vel-src", "vel-curr" ) );
	solverPreCalc->addPlugin ( "set-noslip-bcs", StringArg ( "grid","vel-curr" ) );
	solverPreCalc->addPlugin ( "diffuse-grid", StringArg ( "src-vec3", "vel-curr" ) + RealArg ( "diff", 0.3 ) );
	if (rotate)
		solverPreCalc->addPlugin ("set-moving-obs-bcs", StringArg("obstacle","obstacle-flags") + StringArg("flags-src","empty-flags") +
							VecArg("obs-rot-axis", rotAxis) + RealArg("obs-rot-vel", rotSpeed) + VecArg("obs-center", Vec3(0.5,0.5,0.5)));
	solverPreCalc->addPlugin ( "solve-pressure", IntArg ( "openbound",0 ) );
	if (rotate)
		solverPreCalc->addPlugin ( "average", StringArg ( "gridname","vel-curr" ) + StringArg ( "sumgrid","mean-vel" ) + IntArg ( "from", frames ) + IntArg ( "frames", 3 ) + IntArg ( "post-quit",1 ) + IntArg("stride", frames) );
	else
		solverPreCalc->addPlugin ( "average", StringArg ( "gridname","vel-curr" ) + StringArg ( "sumgrid","mean-vel" ) + IntArg ( "from", frames ) + IntArg ( "frames", frames ) + IntArg ( "post-quit",1 ) );
	
	// program final steps
	solverPreCalc->addEndPlugin ( "calc-abl", StringArg ("mean-vel","mean-vel") + StringArg ("dist","dist") + StringArg("normal","normal") + StringArg("abl","abl") + RealArg("d", 1.7));
	
	if (dynamic)
		solverPreCalc->addEndPlugin ( "add-database", StringArg("grid","abl") + StringArg("normal","normal") + VecArg("u0",rotate ? (rotSpeed*rotAxis):inflow));
	else {
		solverPreCalc->addEndPlugin ( "dump-universal", StringArg ( "grid","abl" ) + StringArg ( "override-name","scene/" + name + "-abl" )  + IntArg ( "single-dump", 1 ) );	
		solverPreCalc->addEndPlugin ( "dump-universal", StringArg ( "grid","mean-vel" ) + StringArg ( "override-name","scene/" + name + "-mean" ) + IntArg ( "single-dump", 1 ) );
	}
		
	_solvers.push_back(solverPreCalc);

	initAllSolvers();

	/* Calculate all requested frames */
	/* DG TODO: Can I precalc one frame at a time instead of all at once? */
	while(!advanceAllSolvers()) {};

	finalizeAllSolvers();

	// _abl = _solvers[0]->getParams().getGridVec3("abl");
	// _meanVel = _solvers[0]->getParams().getGridVec3("mean-vel");

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

	_init = 0;
}