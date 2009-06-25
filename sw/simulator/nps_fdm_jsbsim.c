#include <FGFDMExec.h>
#include <FGJSBBase.h>
#include <FGState.h>
#include <stdlib.h>
#include "nps_fdm.h"
#include "6dof.h"
#include "airframe.h"
#include "pprz_algebra.h"
#include "pprz_algebra_float.h"

using namespace JSBSim;

static void feed_jsbsim(double* commands);
static void fetch_state(void);
static void jsbsimvec_to_vec(DoubleVect3* fdm_vector, const FGColumnVector3* jsb_vector);
static void jsbsimloc_to_loc(EcefCoor_d* fdm_location, FGLocation* jsb_location);
static void jsbsimquat_to_quat(DoubleQuat* fdm_quat, FGQuaternion* jsb_quat);
static void jsbsimvec_to_rate(DoubleRates* fdm_rate, const FGColumnVector3* jsb_vector);
static void rate_to_vec(DoubleVect3* vector, DoubleRates* rate);
static void init_jsbsim(double dt);
static void init_fdm_vars(void);

struct NpsFdm fdm;
FGFDMExec* FDMExec;

void nps_fdm_init(double dt) {

  init_jsbsim(dt);
  init_fdm_vars();
 
}

void nps_fdm_run_step(double* commands) {

  feed_jsbsim(commands);

  FDMExec->Run();

  fetch_state();

}

static void feed_jsbsim(double* commands) {

  char buf[64];
  const char* names[] = NPS_ACTUATOR_NAMES;
  string property;

  int i;
  for (i=0; i<SERVOS_NB; i++) {
    sprintf(buf,"fcs/%s",names[i]);
    property = string(buf);
    FDMExec->GetPropertyManager()->SetDouble(property,commands[i]);
  }
}

static void fetch_state(void) {

  FGGroundReactions* ground_reactions;
  FGPropagate* propagate;
  FGPropagate::VehicleState* VState;
  DoubleVect3 noninertial_accel;
  DoubleVect3 dummy_vector;

  ground_reactions = FDMExec->GetGroundReactions();
  propagate = FDMExec->GetPropagate();
  VState = propagate->GetVState();

  fdm.on_ground = ground_reactions->GetWOW();
  
  jsbsimloc_to_loc(&fdm.ecef_pos,&VState->vLocation);
  jsbsimvec_to_vec(&fdm.body_ecef_vel,&VState->vUVW);
  jsbsimvec_to_vec(&noninertial_accel,&propagate->GetUVWdot());

  jsbsimquat_to_quat(&fdm.ltp_to_body_quat,&VState->vQtrn);
  jsbsimvec_to_rate(&fdm.body_ecef_rotvel,&VState->vPQR);
  jsbsimvec_to_rate(&fdm.body_ecef_rotaccel,&propagate->GetPQRdot());

  rate_to_vec(&dummy_vector,&fdm.body_ecef_rotvel);
  DOUBLE_VECT3_CROSS_PRODUCT(fdm.body_ecef_accel,dummy_vector,fdm.body_ecef_vel);
  DOUBLE_VECT3_SUM(fdm.body_ecef_accel,fdm.body_ecef_accel,noninertial_accel)
  DOUBLE_EULERS_OF_QUAT(fdm.ltp_to_body_eulers, fdm.ltp_to_body_quat);
  
}


static void init_jsbsim(double dt) {

  char buf[1024];
  string rootdir;
  JSBSim::FGState* State;
  
  sprintf(buf,"%s/conf/simulator/jsbsim/",getenv("PAPARAZZI_HOME"));
  rootdir = string(buf);
  FDMExec = new FGFDMExec();
  
  State = FDMExec->GetState();
  State->Setsim_time(0.);
  State->Setdt(dt);
  
  FDMExec->DisableOutput();
  FDMExec->SetDebugLevel(0); // No DEBUG messages
  
  if ( ! FDMExec->LoadModel( rootdir + "aircraft",
			     rootdir + "engine",
			     rootdir + "systems",
			     AIRFRAME_NAME,
			     false)){
#ifdef DEBUG
    cerr << "  JSBSim could not be started" << endl << endl;
#endif
    delete FDMExec;
    exit(-1);
  }
  
  JSBSim::FGInitialCondition *IC = FDMExec->GetIC();
  if ( ! IC->Load(NPS_INITIAL_CONDITITONS)) {
#ifdef DEBUG
    cerr << "Initialization unsuccessful" << endl;
#endif
    delete FDMExec;
    exit(-1);
  }

}

static void init_fdm_vars(void) {

  fdm.on_ground = false;
  
  FLOAT_VECT3_ZERO(fdm.ecef_pos);
  FLOAT_VECT3_ZERO(fdm.body_ecef_vel);
  FLOAT_VECT3_ZERO(fdm.body_ecef_accel);
  
  FLOAT_QUAT_ZERO(fdm.ltp_to_body_quat);
  FLOAT_RATES_ZERO(fdm.body_ecef_rotvel);   
  FLOAT_RATES_ZERO(fdm.body_ecef_rotaccel);

  VECT3_ASSIGN(fdm.ltp_g, 0., 0., 9.81);
  VECT3_ASSIGN(fdm.ltp_g, 1., 0., 1.);

 }

static void jsbsimloc_to_loc(EcefCoor_d* fdm_location, FGLocation* jsb_location){

  fdm_location->x = jsb_location->Entry(1);
  fdm_location->y = jsb_location->Entry(2);
  fdm_location->z = jsb_location->Entry(3);
  
}
 
static void jsbsimvec_to_vec(DoubleVect3* fdm_vector, const FGColumnVector3* jsb_vector) {

  fdm_vector->x = jsb_vector->Entry(1);
  fdm_vector->y = jsb_vector->Entry(2);
  fdm_vector->z = jsb_vector->Entry(3);

}

static void jsbsimquat_to_quat(DoubleQuat* fdm_quat, FGQuaternion* jsb_quat){
  
  fdm_quat->qi = jsb_quat->Entry(1);
  fdm_quat->qx = jsb_quat->Entry(2);
  fdm_quat->qy = jsb_quat->Entry(3);
  fdm_quat->qz = jsb_quat->Entry(4);

}

static void jsbsimvec_to_rate(DoubleRates* fdm_rate, const FGColumnVector3* jsb_vector) {

  fdm_rate->p = jsb_vector->Entry(1);
  fdm_rate->q = jsb_vector->Entry(2);
  fdm_rate->r = jsb_vector->Entry(3);

}


static void rate_to_vec(DoubleVect3* vector, DoubleRates* rate) {

  vector->x = rate->p;
  vector->y = rate->q;
  vector->z = rate->r;

}

