#include "iPic3D.h"
#include "MyClock.h"

using namespace iPic3D;

extern MyClock *clocks;

int c_Solver::Init(int argc, char **argv) {

  MlmdVerbose= true;
  /* wether to print the distribution function */
  WriteDistrFun= true;
  WriteDistrFun_RepPart= true;

  // initialize MPI environment
  // nprocs = number of processors
  // myrank = rank of tha process*/
  mpi = new MPIdata(&argc, &argv);
  // mlmd: i need the rank at grid level, not on MPI_COMM_WORLD -> use vct rather than mpi
  //nprocs = mpi->nprocs;
  //myrank = mpi->rank;

  // random size
  clocks = new MyClock(20);
  clocks->start(0);

  col = new Collective(argc, argv); // Every proc loads the parameters of simulation from class Collective

  /*MPI_Barrier(MPI_COMM_WORLD);
  cout << "after the collective init" << endl;
  cout << "exiting now..."<< endl;
  mpi->finalize_mpi(); exit(EXIT_SUCCESS);*/

  verbose = col->getVerbose();
  restart_cycle = col->getRestartOutputCycle();
  SaveDirName = col->getSaveDirName();
  RestartDirName = col->getRestartDirName();
  restart = col->getRestart_status();
  ns = col->getNs();            // get the number of particle species involved in simulation
  first_cycle = col->getLast_cycle() + 1; // get the last cycle from the restart

  /* mlmd: to decide whether to perform mlmd ops */
  MLMD_BC = col->getMLMD_BC();
  MLMD_PROJECTION = col->getMLMD_PROJECTION();
  MLMD_ParticleREPOPULATION = col->getMLMD_ParticleREPOPULATION();
  // the timing of particle repopulation ops depends on this
  FluidLikeRep= col->getFluidLikeRep();
  // wether to repopulate before or after the mover
  RepopulateBeforeMover= col->getRepopulateBeforeMover();

  //MLMD_InitialInterpolation = col->getMLMD_InitialInterpolation();
  /* end mlmd: to decide whether to perform mlmd ops */
  
  // initialize the virtual cartesian topology 
  vct = new VCtopology3D(col);
  /* pre-mlmd: We create a new communicator with a 3D virtual Cartesian topology
     vct->setup_vctopology(MPI_COMM_WORLD);
     mlmd: severely modified */

  vct->setup_vctopology(MPI_COMM_WORLD, col);

  /*if (MlmdVerbose){
    vct->testMlmdCommunicators();
    }*/

  nprocs = vct->getNprocs();  // @gridLevel
  myrank = vct->getCartesian_rank(); // @gridLevel
  numGrid = vct->getNumGrid(); // added, mlmd
  
  if (myrank==0){
    cout << "I am grid " << numGrid << ", running on " << nprocs << "cores" << endl; }

  if (RepopulateBeforeMover and myrank==0)
    cout << "I am repopulating before mover " << endl;

  // Check if we can map the processes into a matrix ordering defined in Collective.cpp
 
  if (nprocs != vct->getNprocs()) {
    if (myrank == 0) {
      cerr << "Error: " << nprocs << " processes cant be mapped into " << vct->getXLEN() << "x" << vct->getYLEN() << "x" << vct->getZLEN() << " matrix: Change XLEN,YLEN, ZLEN in method VCtopology3D.init()" << endl;
      mpi->finalize_mpi();
      return (1);
    }
  }

#ifdef BATSRUS
  // set index offset for each processor
  col->setGlobalStartIndex(vct);
#endif

 
  // Create the local grid
  grid = new Grid3DCU(col, vct);  // Create the local grid
  /*! pre-mlmd: no topology needed in the fields constructor
    mlmd: now i need the topology also*/
  //EMf = new EMfields3D(col, grid);  // Create Electromagnetic Fields Object
  EMf = new EMfields3D(col, grid, vct);  // Create Electromagnetic Fields Object

  nx0 = col->getNxc_mlmd(numGrid) / vct->getXLEN(); // get the number of cells in x for each processor
  ny0 = col->getNyc_mlmd(numGrid) / vct->getYLEN(); // get the number of cells in y for each processor
  nz0 = col->getNzc_mlmd(numGrid) / vct->getZLEN(); // get the number of cells in z for each processor
  // Print the initial settings to stdout and a file
  if (myrank == 0) {
    mpi->Print();
    vct->Print();
    col->Print();
    col->save();
  }


  if (col->getSolInit()) {
    /* -------------------------------------------- */
    /* If using parallel H5hut IO read initial file */
    /* -------------------------------------------- */
    ReadFieldsH5hut(ns, false, EMf, col, vct, grid);

  }
  else {
    /* --------------------------------------------------------- */
    /* If using 'default' IO initialize fields depending on case */
    /* --------------------------------------------------------- */
    if      (col->getCase()=="GEMnoPert") EMf->initGEMnoPert(vct,grid,col);
    else if (col->getCase()=="ForceFree") EMf->initForceFree(vct,grid,col);
    else if (col->getCase()=="GEM")       EMf->initGEM(vct, grid,col);
    else if (col->getCase()=="BATSRUS")   EMf->initBATSRUS(vct,grid,col);
    else if (col->getCase()=="Dipole")    EMf->init(vct,grid,col);
    else if (col->getCase()=="LightWave") EMf->initLightWave(vct,grid, col);
    else if (col->getCase()=="DoubleGEM") EMf->initDoubleGEM(vct,grid, col);
    else if (col->getCase()=="DoubleGEM_CentralPerturbation") EMf->initDoubleGEM_CentralPerturbation(vct,grid, col);
    else if (col->getCase()=="initTestProjection") EMf->initTestProjection(vct,grid, col);
    else if (col->getCase()=="initTestBC") EMf->initTestBC(vct,grid, col);
    else if (col->getCase()=="TestFix3B") EMf->initTestFix3B(vct,grid, col);
    else if (col->getCase()=="MAX_Show_RG_BC" ) EMf->initMAX_Show_RG_BC(vct, grid, col);
    else if (col->getCase()=="TestBBoundary" ) EMf->initTestBBoundary(vct,grid, col);
    else if (col->getCase()=="initTestIntProj") EMf->initTestIntProj(vct, grid, col);
    else {
      if (myrank==0) {
        cout << " =========================================================== " << endl;
        cout << " WARNING: The case '" << col->getCase() << "' was not recognized. " << endl;
        cout << "          Runing simulation with the default initialization. " << endl;
        cout << " =========================================================== " << endl;
      }
      EMf->init(vct,grid,col);
    }
  }

  // added
  EMf->SetLambda(grid, vct);

  EMf->PostInit();


  MPI_Barrier(MPI_COMM_WORLD); // leave it here if init conditions for RG are interpolated
  int rr= vct->getCartesian_rank();

  // if (MLMD_InitialInterpolation){
  if (col->getMLMD_InitialInterpolation()) { 

    EMf->initWeightBC_InitialInterpolation(grid, vct);
        
    /*if (rr==0){
      cout << "Grid " << numGrid << " after initWeightBC_InitialInterpolation" <<endl;
      }*/
    
    EMf->receiveInitialInterpolation(grid, vct);
    EMf->sendInitialInterpolation(grid, vct);
    
    EMf->ApplyInitialInterpolation(vct, grid);
    EMf->DeallocateII();
      
    if (rr==0){
      cout << "Grid " << numGrid << " after setting interpolated fields" <<endl; 
    }
  }// end if (InitialInterpolations)
  // mlmd BC init
  if (MLMD_BC)
    EMf->initWeightBC(grid, vct);

  MPI_Barrier(MPI_COMM_WORLD);

  if (MLMD_PROJECTION)
    EMf->initWeightProj(grid, vct);
  
  MPI_Barrier(MPI_COMM_WORLD);
  if (rr==0){
    cout << "Grid " << numGrid << " after initWeightBC, initWeightProj" <<endl; 
  }
  
#ifdef __PETSC_SOLVER__
  // PETSc solver:
  petscSolver = new PetscSolver(EMf, grid, vct, col);
#endif
  
  // OpenBC
  EMf->updateInfoFields(grid,vct,col);

  // Allocation of particles
  part = new Particles3D[ns];
  if (col->getSolInit()) {
    if (col->getPartInit()=="File") ReadPartclH5hut(ns, part, col, vct, grid);
    else {
      if (myrank==0) cout << "WARNING: Particle drift velocity from ExB " << endl;
      for (int i = 0; i < ns; i++){
        part[i].allocate(i, 0, col, vct, grid);
        if (col->getPartInit()=="EixB") part[i].MaxwellianFromFields(grid, EMf, vct);
        else                            part[i].maxwellian(grid, EMf, vct);
      }
    }
  }
  else {
    for (int i = 0; i < ns; i++)
      part[i].allocate(i, 0, col, vct, grid);

    // Initial Condition for PARTICLES if you are not starting from RESTART
    if (restart == 0) {
      // wave = new Planewave(col, EMf, grid, vct);
      // wave->Wave_Rotated(part); // Single Plane Wave
      for (int i = 0; i < ns; i++)
        if      (col->getCase()=="ForceFree") part[i].force_free(grid,EMf,vct);
        else if (col->getCase()=="BATSRUS")   part[i].MaxwellianFromFluid(grid,EMf,vct,col,i);
	else if (col->getPartInit()=="DoubleGEM") part[i].MaxwellianDoubleGEM(grid, EMf, vct, col);
	else if (col->getPartInit()=="MAX_Show_RG_BC") part[i].initMAX_Show_RG_BC(grid, EMf, vct);
	else if (col->getPartInit()=="initTestREPExistence" and numGrid >0) {
	  cout << "Grid " << numGrid << " is not initializaing particles" << endl;
	}
        else                                  part[i].maxwellian(grid, EMf, vct);

    }
  }

  if (MLMD_ParticleREPOPULATION)
    for (int i=0; i< ns; i++ ){
      part[i].initWeightPBC(grid, vct);
      // comment during production
      part[i].CheckAfterInitWeightPBC(vct);
    }

  num_grid_STR << numGrid;  //mlmd  
  num_proc << myrank; // mlmd: @grid level 
  

  if (col->getWriteMethod() == "default") {
    // Initialize the output (simulation results and restart file)
    // PSK::OutputManager < PSK::OutputAdaptor > output_mgr; // Create an Output Manager
    // myOutputAgent < PSK::HDF5OutputAdaptor > hdf5_agent; // Create an Output Agent for HDF5 output
    hdf5_agent.set_simulation_pointers(EMf, grid, vct, mpi, col);
    for (int i = 0; i < ns; ++i)
      hdf5_agent.set_simulation_pointers_part(&part[i]);
    output_mgr.push_back(&hdf5_agent);  // Add the HDF5 output agent to the Output Manager's list
    if (myrank == 0 & restart < 2) {
      /* pre-mlmd
	 hdf5_agent.open(SaveDirName + "/settings.hdf"); */
      hdf5_agent.open(SaveDirName + "/settings_G" + num_grid_STR.str() +".hdf");
      output_mgr.output("collective + total_topology + proc_topology", 0);
      hdf5_agent.close();
      /* pre-mlmd
	 hdf5_agent.open(RestartDirName + "/settings.hdf"); */
      hdf5_agent.open(RestartDirName + "/settings_G" + num_grid_STR.str() + ".hdf");
      output_mgr.output("collective + total_topology + proc_topology", 0);
      hdf5_agent.close();
    }
    // Restart
  
    if (restart == 0) {           // new simulation from input file
      /*! pre-mlmd
	hdf5_agent.open(SaveDirName + "/proc" + num_proc.str() + ".hdf"); */
      hdf5_agent.open(SaveDirName + "/proc" + num_proc.str() +"_G" +  num_grid_STR.str() + ".hdf");
      output_mgr.output("proc_topology ", 0);
      hdf5_agent.close();
    }
    else {                        // restart append the results to the previous simulation 
      /*! pre-mlmd
	hdf5_agent.open_append(SaveDirName + "/proc" + num_proc.str() + ".hdf"); */
      hdf5_agent.open_append(SaveDirName + "/proc" + num_proc.str() + "_G" + num_grid_STR.str() + ".hdf");
      output_mgr.output("proc_topology ", 0);
      hdf5_agent.close();
    }
  }
  

  Eenergy, Benergy, TOTenergy = 0.0, TOTmomentum = 0.0;
  Ke = new double[ns];
  momentum = new double[ns];
  /*! pre-mlmd
    cq = SaveDirName + "/ConservedQuantities.txt"; */
  cq = SaveDirName + "/ConservedQuantities_G" + num_grid_STR.str() + ".txt";
  if (myrank == 0) {
    ofstream my_file(cq.c_str());
    my_file.close();
  }
  
  // Distribution functions
  if (WriteDistrFun){
    nDistributionBins = 200; //1000;
    VelocityDist = new unsigned long [nDistributionBins];  
    ds = new string[ns];
    for (int is=0; is<ns; is++){
      stringstream is_STR;
      is_STR << is;
      ds[is] = SaveDirName + "/DistributionFunctions_G"  +num_grid_STR.str() +"_sp" + is_STR.str() + ".txt";
      if (myrank == 0) {
	ofstream my_file(ds[is].c_str());
	my_file.close();
      }
    }
  }
  // distribution function of repopulated particles - using the same bin # and v max as the 'general' d.f.
  if (WriteDistrFun_RepPart and numGrid >0){

    // distribution function of the repopulated particles
    VelocityDist_RepPart = new unsigned long [nDistributionBins];
    ds_RepPart = new string[ns];
    
    for (int is=0; is<ns; is++){
      stringstream is_STR;
      is_STR << is;
      ds_RepPart[is] = SaveDirName + "/DistributionFunctions_RepPart_G"  +num_grid_STR.str() +"_sp" + is_STR.str() + ".txt";
      if (myrank == 0) {
	ofstream my_file(ds_RepPart[is].c_str());
	my_file.close();
      } 
    } 
    
    // distribution function of the NON-repopulated particles
    VelocityDist_NonRepPart = new unsigned long [nDistributionBins];
    ds_NonRepPart = new string[ns];
    
    for (int is=0; is<ns; is++){
      stringstream is_STR;
      is_STR << is;
      ds_NonRepPart[is] = SaveDirName + "/DistributionFunctions_NonRepPart_G"  +num_grid_STR.str() +"_sp" + is_STR.str() + ".txt";
      if (myrank == 0) {
	ofstream my_file(ds_NonRepPart[is].c_str());
	my_file.close();
      } 
    }
    
  } //end if (WriteDistrFun_RepPart and numGrid >0){
  cqsat = SaveDirName + "/VirtualSatelliteTraces_G" +num_grid_STR.str() +"_" + num_proc.str() + ".txt";
  // if(myrank==0){
  ofstream my_file(cqsat.c_str(), fstream::binary);
  nsat = 3;
  for (int isat = 0; isat < nsat; isat++) {
    for (int jsat = 0; jsat < nsat; jsat++) {
      for (int ksat = 0; ksat < nsat; ksat++) {
        int index1 = 1 + isat * nx0 / nsat + nx0 / nsat / 2;
        int index2 = 1 + jsat * ny0 / nsat + ny0 / nsat / 2;
        int index3 = 1 + ksat * nz0 / nsat + nz0 / nsat / 2;
        my_file << grid->getXC(index1, index2, index3) << "\t" << grid->getYC(index1, index2, index3) << "\t" << grid->getZC(index1, index2, index3) << endl;
      }}}
  my_file.close();


  Qremoved = new double[ns];

  my_clock = new Timing(myrank);

  int RR= vct->getSystemWide_rank();
  MPI_Barrier(MPI_COMM_WORLD);
  
  if (RR==0){
    cout << "Init finished "<< endl;
  }	

  clocks->stop(0);
			   
  return 0;
}

// with mlmd ops
void c_Solver::GatherMoments(){
  // timeTasks.resetCycle();
  // interpolation
  // timeTasks.start(TimeTasks::MOMENTS);


  EMf->updateInfoFields(grid,vct,col);
  EMf->setZeroDensities();                  // set to zero the densities

  // if particle repopulation is fluid, ops are done here
  if (MLMD_ParticleREPOPULATION and FluidLikeRep){
    for (int i=0; i<ns; i ++){
      part[i].ReceiveFluidBC(grid, vct);
    }
    for (int i=0; i< ns; i++){
      part[i].ApplyFluidPBC(grid, vct, EMf);
    }
  }


  for (int i = 0; i < ns; i++)
    part[i].interpP2G(EMf, grid, vct);      // interpolate Particles to Grid(Nodes)

  // if particle repopulation is fluid, ops are done here
  if (MLMD_ParticleREPOPULATION and FluidLikeRep){
    for (int i=0; i<ns; i ++){
      part[i].SendFluidPBC(grid, vct, EMf);
    }
  }


  EMf->sumOverSpecies(vct);                 // sum all over the species
  //
  // Fill with constant charge the planet
  if (col->getCase()=="Dipole") {
    EMf->ConstantChargePlanet(grid, vct, col->getL_square(),col->getx_center(),col->gety_center(),col->getz_center());
  }

  // EMf->ConstantChargeOpenBC(grid, vct);     // Set a constant charge in the OpenBC boundaries

}

// without mlmd ops
void c_Solver::GatherMoments_Init(){
  // timeTasks.resetCycle();
  // interpolation
  // timeTasks.start(TimeTasks::MOMENTS);


  EMf->updateInfoFields(grid,vct,col);
  EMf->setZeroDensities();                  // set to zero the densities

  for (int i = 0; i < ns; i++)
    part[i].interpP2G(EMf, grid, vct);      // interpolate Particles to Grid(Nodes)

  EMf->sumOverSpecies(vct);                 // sum all over the species
  //
  // Fill with constant charge the planet
  if (col->getCase()=="Dipole") {
    EMf->ConstantChargePlanet(grid, vct, col->getL_square(),col->getx_center(),col->gety_center(),col->getz_center());
  }

  // EMf->ConstantChargeOpenBC(grid, vct);     // Set a constant charge in the OpenBC boundaries

}

void c_Solver::UpdateCycleInfo(int cycle) {

  EMf->UpdateCycle(cycle);
  return;


  EMf->UpdateFext(cycle);
  if (myrank == 0) cout << " Fext = " << EMf->getFext() << endl;
  if (cycle == first_cycle) {
    if (col->getCase()=="Dipole") {
      EMf->SetDipole_2Bext(vct,grid,col);
      EMf->SetLambda(grid, vct);
    }
  }


}

void c_Solver::CalculateField() {

  // timeTasks.resetCycle();
  // interpolation
  // timeTasks.start(TimeTasks::MOMENTS);

  EMf->interpDensitiesN2C(vct, grid);       // calculate densities on centers from nodes
  EMf->calculateHatFunctions(grid, vct);    // calculate the hat quantities for the implicit method
  /*! pre-mlmd: used to be a barrier on MPI_COMM_WORLD
    mlmd: I need a barrier on the local grid */
  //MPI_Barrier(MPI_COMM_WORLD);
  MPI_Barrier(vct->getCommGrid()); 
  // timeTasks.end(TimeTasks::MOMENTS);

  /* mlmd: BC */
  // receive BC on En+theta, Bn 
  if (MLMD_BC) {EMf->receiveBC(grid, vct);}
  /* end mlmd: BC */


  if (true){
    // MAXWELL'S SOLVER
    // timeTasks.start(TimeTasks::FIELDS);
#ifdef __PETSC_SOLVER__
    petscSolver->solveE();
#else
    EMf->calculateE(grid, vct, col);               // calculate the E field
#endif
    // timeTasks.end(TimeTasks::FIELDS);
  }

  /* mlmd: BC */
  // send BC on En+theta, Bn 
  if (MLMD_BC) {EMf->sendBC(grid, vct);}
  /* end mlmd: BC */

  //MPI_Barrier(vct->getComm());
  // if you are are child, send projection, E n+theta                           
  if (MLMD_PROJECTION){
    EMf->sendProjection(grid,vct);
  
    EMf->receiveProjection(grid,vct);
    //EMf->TestProjection(grid, vct); // This has to be on only if you are testing, without running
    EMf->applyProjection(grid, vct, col);
  }
}

void c_Solver::CalculateBField(int cycle) {
  /* --------------------- */
  /* Calculate the B field */
  /* --------------------- */

  // timeTasks.start(TimeTasks::BFIELD);


  // receive BC on  Bn 
  //if (MLMD_BC) {EMf->receiveBC(grid, vct);}

  EMf->calculateB(grid, vct, col, cycle);   // calculate the B field

  // send BC on Bn 
  //if (MLMD_BC) {EMf->sendBC(grid, vct);}

  // timeTasks.end(TimeTasks::BFIELD);

  // print out total time for all tasks
  // timeTasks.print_cycle_times();

}

bool c_Solver::ParticlesMover(int cycle) {
  
  /*  -------------- */
  /*  Particle mover */
  /*  -------------- */

  if (ns==0) mem_avail=1; // otherwise crashes

  if (RepopulateBeforeMover){
    for (int i=0; i<ns; i++){
      // CG: send particle BC
      part[i].SendPBC(grid, vct);
      // RG: delete particles in PRA area
      part[i].communicateAfterMover(vct);
      // RG: accept BC particles in PRA area
      part[i].ReceivePBC(grid, vct, cycle);
    }
  }

  #ifdef __PROFILING__
  clocks->start(6);  
  #endif

  for (int i = 0; i < ns; i++)  // move each species
  {
    // #pragma omp task inout(part[i]) in(grid) target_device(booster)
    mem_avail = part[i].mover_PC_sub(grid, vct, EMf); // use the Predictor Corrector scheme 
  }
  #ifdef __PROFILING__
  clocks->stop(6);
  #endif

  if (mem_avail < 0) {          // not enough memory space allocated for particles: stop the simulation
    if (myrank == 0) {
      cout << "*************************************************************" << endl;
      cout << "Simulation stopped. Not enough memory allocated for particles" << endl;
      cout << "*************************************************************" << endl;
    }
    return (true);              // exit from the time loop
  }

  /* -------------------------------------- */
  /* Repopulate the buffer zone at the edge */
  /* -------------------------------------- */

  InjectBoundaryParticles();

  if (mem_avail < 0) {          // not enough memory space allocated for particles: stop the simulation
    if (myrank == 0) {
      cout << "*************************************************************" << endl;
      cout << "Simulation stopped. Not enough memory allocated for particles" << endl;
      cout << "*************************************************************" << endl;
    }
    return (true);              // exit from the time loop
  }
  // CG sends PBC
  int RR= vct->getCartesian_rank();   

  // particle repopulation ops are done here if the repopulation is kinetic
  #ifdef __PROFILING__
  clocks->start(7);
  #endif

  if (MLMD_ParticleREPOPULATION and !FluidLikeRep and !RepopulateBeforeMover){
    for (int i = 0; i < ns; i++){
      // in practice, removes particles from the PRA
      #ifdef __PROFILING__
      clocks->start(8);
      #endif
      part[i].communicateAfterMover(vct);
      #ifdef __PROFILING__
      clocks->stop(8);
      #endif

      #ifdef __PROFILING__
      clocks->start(9);
      #endif
      part[i].SendPBC(grid, vct);
      #ifdef __PROFILING__
      clocks->stop(9);
      #endif

      #ifdef __PROFILING__
      clocks->start(10);
      #endif
      part[i].ReceivePBC(grid, vct, cycle);
      #ifdef __PROFILING__
      clocks->stop(10);
      #endif
      // comment during production
      //part[i].CheckSentReceivedParticles(vct);

      // this one is needed only if AllowPMsgResize=1 
      // (see notes in postEPS2017.rtfd)
      
      #ifdef __PROFILING__
      clocks->start(11);
      #endif
      if (part[i].getAllowPMsgResize()){
	part[i].MPI_Barrier_ParentChild(vct);
      }
      #ifdef __PROFILING__
      clocks->stop(11);
      #endif
    }
  }
  #ifdef __PROFILING__
  clocks->stop(7);
  #endif

  // just to delete particles
  if (MLMD_ParticleREPOPULATION and FluidLikeRep and !RepopulateBeforeMover){
    for (int i = 0; i < ns; i++){
      // in practice, removes particles from the PRA
      part[i].communicateAfterMover(vct);
    }
  }

  

 
  return (false);

}

void c_Solver::InjectBoundaryParticles(){
  
  if (numGrid >0) return;

  for (int i=0; i < ns; i++) {
    if (col->getRHOinject(i)>0.0){

      mem_avail = part[i].particle_repopulator(grid,vct,EMf,i);

      /* --------------------------------------- */
      /* Remove particles from depopulation area */
      /* --------------------------------------- */

      if (col->getCase()=="Dipole") {
        for (int i=0; i < ns; i++)
          Qremoved[i] = part[i].deleteParticlesInsideSphere(col->getL_square(),col->getx_center(),col->gety_center(),col->getz_center());

      }
    }
  }

}

void c_Solver::WriteRestart(int cycle) {
  // write the RESTART file
  if (cycle % restart_cycle == 0 && cycle != first_cycle) {
    if (col->getWriteMethod() != "h5hut") {
      // without ,0 add to restart file
      writeRESTART(RestartDirName, myrank, cycle, ns, mpi, vct, col, grid, EMf, part, 0);
    }
  }

}

void c_Solver::WriteConserved(int cycle) {
  // write the conserved quantities
  if (cycle % col->getDiagnosticsOutputCycle() == 0) {
    /*! mlmd: i need the communicator also
    Eenergy = EMf->getEenergy();
    Benergy = EMf->getBenergy();
    */
    Eenergy = EMf->getEenergy(vct->getCommGrid());
    Benergy = EMf->getBenergy(vct->getCommGrid());
    TOTenergy = 0.0;
    TOTmomentum = 0.0;
    for (int is = 0; is < ns; is++) {
      // mlmd
      //Ke[is] = part[is].getKe();
      Ke[is] = part[is].getKe(vct->getCommGrid()); 
      TOTenergy += Ke[is];
      //momentum[is] = part[is].getP();
      momentum[is] = part[is].getP(vct->getCommGrid());
      TOTmomentum += momentum[is];
    }
    if (myrank == 0) {
      ofstream my_file(cq.c_str(), fstream::app);
      my_file << cycle << "\t" << "\t" << (Eenergy + Benergy + TOTenergy) << "\t" << TOTmomentum << "\t" << Eenergy << "\t" << Benergy << "\t" << TOTenergy <<"\t" ;
      for (int is=0; is< ns; is++){
	my_file << Ke[is] << "\t";
      }
      my_file << endl;

      my_file.close();
    }

    if (WriteDistrFun){
      // Velocity distribution
      for (int is = 0; is < ns; is++) {
	// it actually returns the max u^2 + v^2 + w^3
	// per Grid
	//double maxVel = part[is].getMaxVelocity(vct->getCommGrid());
	// use the same maxVel in the re system
	//double maxVel = part[is].getMaxVelocity(MPI_COMM_WORLD);
	// use the same max vel at all times (to compare d.f. without normalizing)
	double maxVel= col->getUth(is)*7;
	VelocityDist = part[is].getVelocityDistribution(nDistributionBins, maxVel, vct->getCommGrid());
	if (myrank == 0) {
	  ofstream my_file(ds[is].c_str(), fstream::app);
	  my_file << cycle << "\t" << is << "\t" << maxVel;
	  for (int i = 0; i < nDistributionBins; i++)
	    my_file << "\t" << VelocityDist[i];
	  my_file << endl;
	  my_file.close();
	}
      }

    } // end if (WriteDistrFun)
    if (WriteDistrFun_RepPart and numGrid >0){
      // this the repopulated particles
      for (int is = 0; is < ns; is++) {
	double maxVel= col->getUth(is)*7;
	VelocityDist_RepPart = part[is].getVelocityDistribution_RepPart(nDistributionBins, maxVel, vct->getCommGrid());
	if (myrank == 0) {
	  ofstream my_file(ds_RepPart[is].c_str(), fstream::app);
	  my_file << cycle << "\t" << is << "\t" << maxVel;
	  for (int i = 0; i < nDistributionBins; i++)
	    my_file << "\t" << VelocityDist_RepPart[i];
	  my_file << endl;
	  my_file.close();
	}
      }

      // this the NON repopulated particles
      for (int is = 0; is < ns; is++) {
	double maxVel= col->getUth(is)*7;
	VelocityDist_NonRepPart = part[is].getVelocityDistribution_NonRepPart(nDistributionBins, maxVel, vct->getCommGrid());
	if (myrank == 0) {
	  ofstream my_file(ds_NonRepPart[is].c_str(), fstream::app);
	  my_file << cycle << "\t" << is << "\t" << maxVel;
	  for (int i = 0; i < nDistributionBins; i++)
	    my_file << "\t" << VelocityDist_NonRepPart[i];
	  my_file << endl;
	  my_file.close();
	}
      }
      
    } // end if (WriteDistrFun_RepPart)

  } // end check on diagnostic cycle
  
  //if (cycle%(col->getFieldOutputCycle())==0){
  //  for (int is = 0; is < ns; is++) {
  //    part[is].Add_vDist3D();
  //    part[is].Write_vDist3D(SaveDirName);
  //  }
  //}
}

void c_Solver::WriteOutput(int cycle) {

  /*! NB: num_proc is my_rank, hence local to the grid */

  if (col->getWriteMethod() == "h5hut") {

    /* -------------------------------------------- */
    /* Parallel HDF5 output using the H5hut library */
    /* -------------------------------------------- */

    if (cycle%(col->getFieldOutputCycle())==0)        WriteFieldsH5hut(ns, grid, EMf,  col, vct, cycle);
    if (cycle%(col->getParticlesOutputCycle())==0 &&
        cycle!=col->getLast_cycle() && cycle!=0)      WritePartclH5hut(ns, grid, part, col, vct, cycle);

  }
  else
  {

    // OUTPUT to large file, called proc**
    if (cycle % (col->getFieldOutputCycle()) == 0 || cycle == first_cycle) {
      /*! pre-mlmd
	hdf5_agent.open_append(SaveDirName + "/proc" + num_proc.str() + ".hdf"); */
      hdf5_agent.open_append(SaveDirName + "/proc" + num_proc.str() +"_G"+ num_grid_STR.str() + ".hdf");
      output_mgr.output("Eall + Ball + rhos + Jsall + pressure", cycle);
      // Pressure tensor is available
      hdf5_agent.close();
    }
    if (cycle % (col->getParticlesOutputCycle()) == 0 && col->getParticlesOutputCycle() != 1) {
      /*! pre-mlmd
	hdf5_agent.open_append(SaveDirName + "/proc" + num_proc.str() + ".hdf"); */
      hdf5_agent.open_append(SaveDirName + "/proc" + num_proc.str() +"_G"+ num_grid_STR.str() + ".hdf");
      output_mgr.output("position + velocity + q ", cycle, 1);
      hdf5_agent.close();
    }
    // write the virtual satellite traces

    
    if (ns > 2) {
      ofstream my_file(cqsat.c_str(), fstream::app);
      for (int isat = 0; isat < nsat; isat++) {
        for (int jsat = 0; jsat < nsat; jsat++) {
          for (int ksat = 0; ksat < nsat; ksat++) {
            int index1 = 1 + isat * nx0 / nsat + nx0 / nsat / 2;
            int index2 = 1 + jsat * ny0 / nsat + ny0 / nsat / 2;
            int index3 = 1 + ksat * nz0 / nsat + nz0 / nsat / 2;
            my_file << EMf->getBx(index1, index2, index3) << "\t" << EMf->getBy(index1, index2, index3) << "\t" << EMf->getBz(index1, index2, index3) << "\t";
            my_file << EMf->getEx(index1, index2, index3) << "\t" << EMf->getEy(index1, index2, index3) << "\t" << EMf->getEz(index1, index2, index3) << "\t";
            my_file << EMf->getJxs(index1, index2, index3, 0) + EMf->getJxs(index1, index2, index3, 2) << "\t" << EMf->getJys(index1, index2, index3, 0) + EMf->getJys(index1, index2, index3, 2) << "\t" << EMf->getJzs(index1, index2, index3, 0) + EMf->getJzs(index1, index2, index3, 2) << "\t";
            my_file << EMf->getJxs(index1, index2, index3, 1) + EMf->getJxs(index1, index2, index3, 3) << "\t" << EMf->getJys(index1, index2, index3, 1) + EMf->getJys(index1, index2, index3, 3) << "\t" << EMf->getJzs(index1, index2, index3, 1) + EMf->getJzs(index1, index2, index3, 3) << "\t";
            my_file << EMf->getRHOns(index1, index2, index3, 0) + EMf->getRHOns(index1, index2, index3, 2) << "\t";
            my_file << EMf->getRHOns(index1, index2, index3, 1) + EMf->getRHOns(index1, index2, index3, 3) << "\t";
          }}}
      my_file << endl;
      my_file.close();
      }
  }
}

void c_Solver::Finalize() {
  if (mem_avail == 0) {          // write the restart only if the simulation finished succesfully
    if (col->getWriteMethod() != "h5hut") {
      writeRESTART(RestartDirName, myrank, (col->getNcycles() + first_cycle) - 1, ns, mpi, vct, col, grid, EMf, part, 0);
    }
  }

  // stop profiling
  my_clock->stopTiming();

  // deallocate
  delete[]Ke;
  delete[]momentum;
  // close MPI
  mpi->finalize_mpi();
}

void c_Solver::CalculateField_ECSIM(int cycle) {

  //NN if (col->getStaticField()) return;

  //NN MPI_Barrier(MPI_COMM_WORLD);
  MPI_Barrier(vct->getCommGrid());

  /* NN
  // Divergence cleanning                       
  if (col->getPoissonCorrection() == "yes" and (cycle % col->getPoissonFreq()) == 0){
    #ifdef __WITH_PETSC__
    petscSolver_Poisson->solveE();
    #else
    if (myrank == 0) printf("WARNING: Poisson correction not available without PETSc\n");
    #endif
    } */

  /* mlmd: BC */
  // receive BC on En+theta, Bn                                              
  if (MLMD_BC) {EMf->receiveBC(grid, vct);}
  /* end mlmd: BC */

  EMf->calculateEB_ECSIM(grid, vct, col, cycle); 

  /* mlmd: BC */
  // send BC on En+theta, Bn                    
  if (MLMD_BC) {EMf->sendBC(grid, vct);}
  /* end mlmd: BC */
  
  /* NN
  // MAXWELL'S SOLVER            
  #ifdef __WITH_PETSC__
  petscSolver_Maxwell->solveE();
  #else
  EMf->calculateEB(grid, vct, col);               // calculate the E field 
  #endif
  // Impose MHD Buffer Zone                                  
  if (col->getCase()=="MHDUCLA"){
    EMf->setMHDBufferZone(grid, col);
  }

  // Compute the charge density as the divergence of the current   
  #ifdef _CHARGE_FROM_CURRENT_
  //for (int i = 0; i < ns; i++)                             
  //part[i].computeCurrent(EMf, grid, vct);      // interpolate Particles to Grid(Nodes)   
  EMf->chargeFromCurrent(grid, vct);
  #endif
  */

}

void c_Solver::interpBC2N_ECSIM(int cycle){

  EMf->centers2nodesB(vct, grid, col);
  
}
