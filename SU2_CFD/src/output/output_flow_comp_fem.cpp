/*!
 * \file output_direct_mean.cpp
 * \brief Main subroutines for compressible flow output
 * \author R. Sanchez
 * \version 6.0.1 "Falcon"
 *
 * The current SU2 release has been coordinated by the
 * SU2 International Developers Society <www.su2devsociety.org>
 * with selected contributions from the open-source community.
 *
 * The main research teams contributing to the current release are:
 *  - Prof. Juan J. Alonso's group at Stanford University.
 *  - Prof. Piero Colonna's group at Delft University of Technology.
 *  - Prof. Nicolas R. Gauger's group at Kaiserslautern University of Technology.
 *  - Prof. Alberto Guardone's group at Polytechnic University of Milan.
 *  - Prof. Rafael Palacios' group at Imperial College London.
 *  - Prof. Vincent Terrapon's group at the University of Liege.
 *  - Prof. Edwin van der Weide's group at the University of Twente.
 *  - Lab. of New Concepts in Aeronautics at Tech. Institute of Aeronautics.
 *
 * Copyright 2012-2018, Francisco D. Palacios, Thomas D. Economon,
 *                      Tim Albring, and the SU2 contributors.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */


#include "../../include/output/output_flow_comp_fem.hpp"

CFlowCompFEMOutput::CFlowCompFEMOutput(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned short val_iZone) : CFlowOutput(config) {

  nDim = geometry->GetnDim();  
  
  nVar = solver[FLOW_SOL]->GetnVar();
  
  turb_model = config->GetKind_Turb_Model();
  
  grid_movement = config->GetGrid_Movement(); 
  
  su2double Gas_Constant, Mach2Vel, Mach_Motion;
  unsigned short iDim;
  su2double Gamma = config->GetGamma();
      
  /*--- Set the non-dimensionalization for coefficients. ---*/
  
  RefArea = config->GetRefArea();
  
  if (grid_movement) {
    Gas_Constant = config->GetGas_ConstantND();
    Mach2Vel = sqrt(Gamma*Gas_Constant*config->GetTemperature_FreeStreamND());
    Mach_Motion = config->GetMach_Motion();
    RefVel2 = (Mach_Motion*Mach2Vel)*(Mach_Motion*Mach2Vel);
  }
  else {
    RefVel2 = 0.0;
    for (iDim = 0; iDim < nDim; iDim++)
      RefVel2  += solver[FLOW_SOL]->GetVelocity_Inf(iDim)*solver[FLOW_SOL]->GetVelocity_Inf(iDim);
  }
  RefDensity  = solver[FLOW_SOL]->GetDensity_Inf();
  RefPressure = solver[FLOW_SOL]->GetPressure_Inf();
  factor = 1.0 / (0.5*RefDensity*RefArea*RefVel2);
  
  /*--- Set the default history fields if nothing is set in the config file ---*/
  
  if (nRequestedHistoryFields == 0){
    RequestedHistoryFields.push_back("ITER");
    RequestedHistoryFields.push_back("RMS_RES");
    nRequestedHistoryFields = RequestedHistoryFields.size();
  }
  if (nRequestedScreenFields == 0){
    if (config->GetTime_Domain()) RequestedScreenFields.push_back("TIME_ITER");
    if (multizone) RequestedScreenFields.push_back("OUTER_ITER");
    RequestedScreenFields.push_back("INNER_ITER");
    RequestedScreenFields.push_back("RMS_DENSITY");
    RequestedScreenFields.push_back("RMS_MOMENTUM-X");
    RequestedScreenFields.push_back("RMS_MOMENTUM-Y");
    RequestedScreenFields.push_back("RMS_ENERGY");
    nRequestedScreenFields = RequestedScreenFields.size();
  }
  if (nRequestedVolumeFields == 0){
    RequestedVolumeFields.push_back("COORDINATES");
    RequestedVolumeFields.push_back("SOLUTION");
    RequestedVolumeFields.push_back("PRIMITIVE");
    nRequestedVolumeFields = RequestedVolumeFields.size();
  }
  
  stringstream ss;
  ss << "Zone " << config->GetiZone() << " (Comp. Fluid)";
  MultiZoneHeaderString = ss.str();
  
  /*--- Use FEM merging routines --- */
  
  fem_output = true;
    
  /*--- Set the volume filename --- */
  
  VolumeFilename = config->GetFlow_FileName();
  
  /*--- Set the surface filename --- */
  
  SurfaceFilename = config->GetSurfFlowCoeff_FileName();
  
  /*--- Set the restart filename --- */
  
  RestartFilename = config->GetRestart_FlowFileName();
  
}

CFlowCompFEMOutput::~CFlowCompFEMOutput(void) {

  if (rank == MASTER_NODE){
    HistFile.close();

  }


}



void CFlowCompFEMOutput::SetHistoryOutputFields(CConfig *config){
  
  /// BEGIN_GROUP: ITERATION, DESCRIPTION: Iteration identifier.
  /// DESCRIPTION: The time iteration index.
  AddHistoryOutput("TIME_ITER",     "Time_Iter",  FORMAT_INTEGER, "ITER"); 
  /// DESCRIPTION: The outer iteration index.
  AddHistoryOutput("OUTER_ITER",   "Outer_Iter",  FORMAT_INTEGER, "ITER"); 
  /// DESCRIPTION: The inner iteration index.
  AddHistoryOutput("INNER_ITER",   "Inner_Iter", FORMAT_INTEGER,  "ITER"); 
  /// END_GROUP
  
  /// DESCRIPTION: Currently used wall-clock time.
  AddHistoryOutput("PHYS_TIME",   "Time(min)", FORMAT_SCIENTIFIC, "PHYS_TIME"); 

  /// BEGIN_GROUP: RMS_RES, DESCRIPTION: The root-mean-square residuals of the SOLUTION variables. 
  /// DESCRIPTION: Root-mean square residual of the density.
  AddHistoryOutput("RMS_DENSITY",    "rms[Rho]",  FORMAT_FIXED,   "RMS_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the momentum x-component.
  AddHistoryOutput("RMS_MOMENTUM-X", "rms[RhoU]", FORMAT_FIXED,   "RMS_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the momentum y-component.
  AddHistoryOutput("RMS_MOMENTUM-Y", "rms[RhoV]", FORMAT_FIXED,   "RMS_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the momentum z-component.
  if (nDim == 3) AddHistoryOutput("RMS_MOMENTUM-Z", "rms[RhoW]", FORMAT_FIXED,   "RMS_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Root-mean square residual of the energy.
  AddHistoryOutput("RMS_ENERGY",     "rms[RhoE]", FORMAT_FIXED,   "RMS_RES", TYPE_RESIDUAL);
  /// END_GROUP
   
  /// BEGIN_GROUP: MAX_RES, DESCRIPTION: The maximum residuals of the SOLUTION variables. 
  /// DESCRIPTION: Maximum residual of the density.
  AddHistoryOutput("MAX_DENSITY",    "max[Rho]",  FORMAT_FIXED,   "MAX_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum x-component. 
  AddHistoryOutput("MAX_MOMENTUM-X", "max[RhoU]", FORMAT_FIXED,   "MAX_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum y-component. 
  AddHistoryOutput("MAX_MOMENTUM-Y", "max[RhoV]", FORMAT_FIXED,   "MAX_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the momentum z-component. 
  if (nDim == 3) AddHistoryOutput("MAX_MOMENTUM-Z", "max[RhoW]", FORMAT_FIXED,   "MAX_RES", TYPE_RESIDUAL);
  /// DESCRIPTION: Maximum residual of the energy.  
  AddHistoryOutput("MAX_ENERGY",     "max[RhoE]", FORMAT_FIXED,   "MAX_RES", TYPE_RESIDUAL);
  /// END_GROUP
  
  /*--- Add analyze surface history fields --- */
  
  AddAnalyzeSurfaceOutput(config);
  
  /*--- Add aerodynamic coefficients fields --- */
  
  AddAerodynamicCoefficients(config);

}

void CFlowCompFEMOutput::SetVolumeOutputFields(CConfig *config){
  
  // Grid coordinates
  AddVolumeOutput("COORD-X", "x", "COORDINATES");
  AddVolumeOutput("COORD-Y", "y", "COORDINATES");
  if (nDim == 3)
    AddVolumeOutput("COORD-Z", "z", "COORDINATES");
  
  // Solution variables
  AddVolumeOutput("DENSITY",    "Density",    "SOLUTION");
  AddVolumeOutput("MOMENTUM-X", "Momentum_x", "SOLUTION");
  AddVolumeOutput("MOMENTUM-Y", "Momentum_y", "SOLUTION");
  if (nDim == 3)
    AddVolumeOutput("MOMENTUM-Z", "Momentum_z", "SOLUTION");
  AddVolumeOutput("ENERGY",     "Energy",     "SOLUTION");  
  
  // Turbulent Residuals
  switch(config->GetKind_Turb_Model()){
  case SST:
    AddVolumeOutput("TKE", "TKE", "SOLUTION");
    AddVolumeOutput("OMEGA", "Omega", "SOLUTION");
    break;
  case SA: case SA_COMP: case SA_E: 
  case SA_E_COMP: case SA_NEG: 
    AddVolumeOutput("NU_TILDE", "Nu_Tilde", "SOLUTION");
    break;
  case NONE:
    break;
  }
  
  // Primitive variables
  AddVolumeOutput("PRESSURE",    "Pressure",                "PRIMITIVE");
  AddVolumeOutput("TEMPERATURE", "Temperature",             "PRIMITIVE");
  AddVolumeOutput("MACH",        "Mach",                    "PRIMITIVE");
  AddVolumeOutput("PRESSURE_COEFF", "Pressure_Coefficient", "PRIMITIVE");
  
  if (config->GetKind_Solver() == FEM_NAVIER_STOKES){
    AddVolumeOutput("LAMINAR_VISCOSITY", "Laminar_Viscosity", "PRIMITIVE"); 
  }
  
  if (config->GetKind_Solver() == FEM_LES || config->GetKind_SGS_Model() != IMPLICIT_LES) {
    AddVolumeOutput("EDDY_VISCOSITY", "Eddy_Viscosity", "PRIMITIVE");
  }
}

void CFlowCompFEMOutput::LoadVolumeDataFEM(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned long iElem, unsigned long index, unsigned short dof){
  
  unsigned short iDim;
  
  /*--- Create an object of the class CMeshFEM_DG and retrieve the necessary
   geometrical information for the FEM DG solver. ---*/

  CMeshFEM_DG *DGGeometry = dynamic_cast<CMeshFEM_DG *>(geometry);

  CVolumeElementFEM *volElem  = DGGeometry->GetVolElem();

  /*--- Get a pointer to the fluid model class from the DG-FEM solver
   so that we can access the states below. ---*/

  CFluidModel *DGFluidModel = solver[FLOW_SOL]->GetFluidModel();
  
  /* Set the pointers for the solution for this element. */

  const unsigned long offset = nVar*volElem[iElem].offsetDOFsSolLocal;
  su2double *solDOFs         = solver[FLOW_SOL]->GetVecSolDOFs() + offset;
  
  /*--- Get the conservative variables for this particular DOF. ---*/

  const su2double *U = solDOFs+dof*nVar;

  /*--- Load the coordinate values of the solution DOFs. ---*/
  
  const su2double *coor = volElem[iElem].coorSolDOFs.data() + dof*nDim;
  
  /*--- Prepare the primitive states. ---*/

  const su2double DensityInv = 1.0/U[0];
  su2double vel[3], Velocity2 = 0.0;
  for(iDim=0; iDim<nDim; ++iDim) {
    vel[iDim] = U[iDim+1]*DensityInv;
    Velocity2 += vel[iDim]*vel[iDim];
  }
  su2double StaticEnergy = U[nDim+1]*DensityInv - 0.5*Velocity2;
  DGFluidModel->SetTDState_rhoe(U[0], StaticEnergy);
  
    
  SetVolumeOutputValue("COORD-X",        index, coor[0]);
  SetVolumeOutputValue("COORD-Y",        index, coor[1]);
  if (nDim == 3) 
    SetVolumeOutputValue("COORD-Z",      index, coor[2]);
  SetVolumeOutputValue("DENSITY",        index, U[0]);
  SetVolumeOutputValue("MOMENTUM-X",     index, U[1]);
  SetVolumeOutputValue("MOMENTUM-Y",     index, U[2]);
  if (nDim == 3){
    SetVolumeOutputValue("MOMENTUM-Z",   index,  U[3]);
    SetVolumeOutputValue("ENERGY",       index,  U[4]);
  } else {                               
    SetVolumeOutputValue("ENERGY",       index,  U[3]);    
  }
  
  SetVolumeOutputValue("PRESSURE",       index, DGFluidModel->GetPressure());
  SetVolumeOutputValue("TEMPERATURE",    index, DGFluidModel->GetTemperature());
  SetVolumeOutputValue("MACH",           index, sqrt(Velocity2)/DGFluidModel->GetSoundSpeed());
  SetVolumeOutputValue("PRESSURE_COEFF", index, DGFluidModel->GetCp());
  
  if (config->GetKind_Solver() == FEM_NAVIER_STOKES){
    SetVolumeOutputValue("LAMINAR_VISCOSITY", index, DGFluidModel->GetLaminarViscosity()); 
  }
  if ((config->GetKind_Solver()  == FEM_LES) && (config->GetKind_SGS_Model() != IMPLICIT_LES)){
    // todo: Export Eddy instead of Laminar viscosity
    SetVolumeOutputValue("EDDY_VISCOSITY", index, DGFluidModel->GetLaminarViscosity()); 
  }
}

void CFlowCompFEMOutput::LoadSurfaceData(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned long iPoint, unsigned short iMarker, unsigned long iVertex){
  

}

void CFlowCompFEMOutput::LoadHistoryData(CConfig *config, CGeometry *geometry, CSolver **solver) {
  
  CSolver* flow_solver = solver[FLOW_SOL];
  
  SetHistoryOutputValue("TIME_ITER",  curr_TimeIter);  
  SetHistoryOutputValue("INNER_ITER", curr_InnerIter);
  SetHistoryOutputValue("OUTER_ITER", curr_OuterIter); 

  SetHistoryOutputValue("RMS_DENSITY", log10(flow_solver->GetRes_RMS(0)));
  SetHistoryOutputValue("RMS_MOMENTUM-X", log10(flow_solver->GetRes_RMS(1)));
  SetHistoryOutputValue("RMS_MOMENTUM-Y", log10(flow_solver->GetRes_RMS(2)));
  if (nDim == 2)
    SetHistoryOutputValue("RMS_ENERGY", log10(flow_solver->GetRes_RMS(3)));
  else {
    SetHistoryOutputValue("RMS_MOMENTUM-Z", log10(flow_solver->GetRes_RMS(3)));
    SetHistoryOutputValue("RMS_ENERGY", log10(flow_solver->GetRes_RMS(4)));
  }
  
  
  SetHistoryOutputValue("MAX_DENSITY", log10(flow_solver->GetRes_Max(0)));
  SetHistoryOutputValue("MAX_MOMENTUM-X", log10(flow_solver->GetRes_Max(1)));
  SetHistoryOutputValue("MAX_MOMENTUM-Y", log10(flow_solver->GetRes_Max(2)));
  if (nDim == 2)
    SetHistoryOutputValue("MAX_ENERGY", log10(flow_solver->GetRes_Max(3)));
  else {
    SetHistoryOutputValue("MAX_MOMENTUM-Z", log10(flow_solver->GetRes_Max(3)));
    SetHistoryOutputValue("MAX_ENERGY", log10(flow_solver->GetRes_Max(4)));
  }
  
  SetHistoryOutputValue("DRAG", flow_solver->GetTotal_CD());
  SetHistoryOutputValue("LIFT", flow_solver->GetTotal_CL());
  if (nDim == 3)
    SetHistoryOutputValue("SIDEFORCE", flow_solver->GetTotal_CSF());
  if (nDim == 3){
    SetHistoryOutputValue("MOMENT-X", flow_solver->GetTotal_CMx());
    SetHistoryOutputValue("MOMENT-Y", flow_solver->GetTotal_CMy());
  }
  SetHistoryOutputValue("MOMENT-Z", flow_solver->GetTotal_CMz());
  SetHistoryOutputValue("FORCE-X", flow_solver->GetTotal_CFx());
  SetHistoryOutputValue("FORCE-Y", flow_solver->GetTotal_CFy());
  if (nDim == 3)
    SetHistoryOutputValue("FORCE-Z", flow_solver->GetTotal_CFz());
  SetHistoryOutputValue("EFFICIENCY", flow_solver->GetTotal_CEff());
  
  for (unsigned short iMarker_Monitoring = 0; iMarker_Monitoring < config->GetnMarker_Monitoring(); iMarker_Monitoring++) {
    SetHistoryOutputPerSurfaceValue("DRAG_ON_SURFACE", flow_solver->GetSurface_CD(iMarker_Monitoring), iMarker_Monitoring);
    SetHistoryOutputPerSurfaceValue("LIFT_ON_SURFACE", flow_solver->GetSurface_CL(iMarker_Monitoring), iMarker_Monitoring);
    if (nDim == 3)
      SetHistoryOutputPerSurfaceValue("SIDEFORCE_ON_SURFACE", flow_solver->GetSurface_CSF(iMarker_Monitoring), iMarker_Monitoring);
    if (nDim == 3){
      SetHistoryOutputPerSurfaceValue("MOMENT-X_ON_SURFACE", flow_solver->GetSurface_CMx(iMarker_Monitoring), iMarker_Monitoring);
      SetHistoryOutputPerSurfaceValue("MOMENT-Y_ON_SURFACE", flow_solver->GetSurface_CMy(iMarker_Monitoring), iMarker_Monitoring);
    }
    SetHistoryOutputPerSurfaceValue("MOMENT-Z_ON_SURFACE", flow_solver->GetSurface_CMz(iMarker_Monitoring), iMarker_Monitoring);
    SetHistoryOutputPerSurfaceValue("FORCE-X_ON_SURFACE", flow_solver->GetSurface_CFx(iMarker_Monitoring), iMarker_Monitoring);
    SetHistoryOutputPerSurfaceValue("FORCE-Y_ON_SURFACE", flow_solver->GetSurface_CFy(iMarker_Monitoring), iMarker_Monitoring);
    if (nDim == 3)
      SetHistoryOutputPerSurfaceValue("FORCE-Z_ON_SURFACE", flow_solver->GetSurface_CFz(iMarker_Monitoring), iMarker_Monitoring);   
    
    SetHistoryOutputPerSurfaceValue("EFFICIENCY_ON_SURFACE", flow_solver->GetSurface_CEff(iMarker_Monitoring), iMarker_Monitoring);
  }
  
  SetHistoryOutputValue("AOA", config->GetAoA());
  SetHistoryOutputValue("LINSOL_ITER", flow_solver->GetIterLinSolver());
  
  /*--- Set the analyse surface history values --- */
  
  SetAnalyzeSurface(flow_solver, geometry, config, false);
  
  /*--- Set aeroydnamic coefficients --- */
  
  SetAerodynamicCoefficients(config, flow_solver);
  
}

su2double CFlowCompFEMOutput::GetQ_Criterion(CConfig *config, CGeometry *geometry, CVariable* node_flow){
  
  unsigned short iDim, jDim;
  su2double Grad_Vel[3][3] = {{0.0, 0.0, 0.0},{0.0, 0.0, 0.0},{0.0, 0.0, 0.0}};
  su2double Omega[3][3]    = {{0.0, 0.0, 0.0},{0.0, 0.0, 0.0},{0.0, 0.0, 0.0}};
  su2double Strain[3][3]   = {{0.0, 0.0, 0.0},{0.0, 0.0, 0.0},{0.0, 0.0, 0.0}};
  for (iDim = 0; iDim < nDim; iDim++) {
    for (unsigned short jDim = 0 ; jDim < nDim; jDim++) {
      Grad_Vel[iDim][jDim] = node_flow->GetGradient_Primitive(iDim+1, jDim);
      Strain[iDim][jDim]   = 0.5*(Grad_Vel[iDim][jDim] + Grad_Vel[jDim][iDim]);
      Omega[iDim][jDim]    = 0.5*(Grad_Vel[iDim][jDim] - Grad_Vel[jDim][iDim]);
    }
  }
  
  su2double OmegaMag = 0.0, StrainMag = 0.0;
  for (iDim = 0; iDim < nDim; iDim++) {
    for (jDim = 0 ; jDim < nDim; jDim++) {
      StrainMag += Strain[iDim][jDim]*Strain[iDim][jDim];
      OmegaMag  += Omega[iDim][jDim]*Omega[iDim][jDim];
    }
  }
  StrainMag = sqrt(StrainMag); OmegaMag = sqrt(OmegaMag);
  
  su2double Q = 0.5*(OmegaMag - StrainMag);
  
  return Q;
}


bool CFlowCompFEMOutput::SetInit_Residuals(CConfig *config){
  
  return (config->GetUnsteady_Simulation() != STEADY && (config->GetIntIter() == 0))|| 
        (config->GetUnsteady_Simulation() == STEADY && (config->GetExtIter() < 2)); 
  
}

bool CFlowCompFEMOutput::SetUpdate_Averages(CConfig *config){
  return false;
  
//  return (config->GetUnsteady_Simulation() != STEADY && !dualtime);
      
}

