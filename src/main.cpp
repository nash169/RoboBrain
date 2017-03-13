/*
 *  main.cpp
 *
 *  This file is part of RoboBrain.
 *  Copyright (C) 2016 Bernardo Fichera
 *
 *  RoboBrain is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RoboBrain is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RoboBrain.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Standard Libraries
#include <algorithm>
#include <ctime>
#include <time.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <string.h>

// My Libraries
#include <dynplot.hh>
#include "environment.hh"

// MPI Simulation
#include <mpi.h>
#include <music.hh>

// Gnuplot Stream
#include <boost/tuple/tuple.hpp>
#include <boost/random.hpp>
#include "gnuplot-iostream.h"


#define IN_LATENCY (0.01)
#define TICK (0.01)

MPI::Intracomm comm;

int main(int argc, char **argv)
{
/*======================================================SETUP PHASE=======================================================*/

/*==================
|   ROBOBEE/ENV    |
==================*/

    const double pi = 3.1415926535897;
    double dynFreq = 1000,
           dynStep = 1/dynFreq,
           reward = 0;


    arma::mat q,
              u(4,1, arma::fill::zeros),
              q_desired;

    // Define Initial & Desired State
    q  << 0.2 << arma::endr << -0.2 << arma::endr << 0 << arma::endr        // Angular Position (Body Attached)
       << 0 << arma::endr << 0 << arma::endr << 1 << arma::endr             // Angular Velocity (Body Attached)
       << 0.04 << arma::endr << 0.04 << arma::endr << 0.01 << arma::endr    // Linear Postion (Inertial Frame)
       << 0.1 << arma::endr << -0.3 << arma::endr << 0;                     // Linear Velocity (Body Attached)

    q_desired << 0 << arma::endr << 0 << arma::endr << 0 << arma::endr      // Angular Position (Body Attached)
              << 0 << arma::endr << 0 << arma::endr << 0 << arma::endr      // Angular Velocity (Body Attached)
              << 0 << arma::endr << 0 << arma::endr << 0.08 << arma::endr   // Linear Postion (Inertial Frame)
              << 0 << arma::endr << 0 << arma::endr << 0;                   // Linear Velocity (Body Attached)

    arma::mat q0 = q;

    // Objects Creation
    Robobee bee(q, dynFreq);     // ROBOBEE
    Controller ctr(q_desired);    // Controller

/*===========================
|   MUSIC Agent Connection  |
===========================*/
    // Create setup object ->  start setup phase
    MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);

    // Read simulation time from configuration music file
    double simt;
    setup->config ("simtime", &simt);

    // Create Input and Output port
    MUSIC::EventInputPort *indata = setup->publishEventInput("p_in");
    MUSIC::EventOutputPort *outdata = setup->publishEventOutput("p_out");

    // Get number of processes and rank processor
    comm = setup->communicator();
    int nProcs = comm.Get_size();
    int rank = comm.Get_rank();

    // Figure out how many input/output channels we have
    int width[2], firstId[2], nLocal[2], rest;
    width[0] = outdata->width();
    width[1] = indata->width();

    // Divide channels evenly over the number of processes
    for (int i = 0; i < 2; ++i)
    {
        nLocal[i] = width[i] / nProcs;
        rest = width[i] % nProcs;
        firstId[i] = nLocal[i] * rank;
        if (rank < rest) {
            firstId[i] += rank;
            nLocal[i] += 1;
        } else
        firstId[i] += rest;
    }

    int idx[nLocal[0]];
    for (int i = 0; i < nLocal[0]; ++i)
        idx[i] = firstId[0] + i;

    // Create an index based on the rank channel asignment above
    MUSIC::LinearIndex outindex(firstId[0], nLocal[0]);
    MUSIC::LinearIndex inindex(firstId[1], nLocal[1]);

    /* NETWORK OUTPUT */
    int pops_size [] = {50, 60, 100}; // [critic_size, actor_size, dopa_size]

    double value_param[] = {1.5, -70.0, 30},       // [A_critic, b_critic, tau_r]
           policy_param[] = {2e-6, -2e-6},          // [F_max, F_min]
           dopa_param[] = {1, 0},                   // [A_dopa, b_dopa]
           *value,
           valueFunction = value_param[1],
           tdError = 0,
           policy = 0,
           dopaActivity = 0;

    // Generate an instance of Receiver
    Receiver *inhandler = new Receiver(pops_size, sizeof(pops_size)/sizeof(pops_size[0]));

    inhandler->SetCritic(0, value_param);
    inhandler->SetActor(1, policy_param);
    inhandler->SetDopa(2, dopa_param);

    /* NETWORK INPUT */
    double max_psg = 500,            // Max rate Poisson process for place cells
           ranges[] = {2*pi, -6*pi}; // Range per state (negative means symmetric with respect to the origin)

    int idState[] = {0,3},           // Define array with States' ID you want to use
        resState[] = {7,7};          // Number of place cells per state

    bool types[] = {true, false};    // true->angle false->anyother

    Sender *outhandler = new Sender(outdata, TICK);
    outhandler->CreatePlaceCells (2, idState, resState, types, ranges, 700);

    // Mapping Input/Output Port
    outdata->map(&outindex, MUSIC::Index::GLOBAL);
    indata->map(&inindex, inhandler, IN_LATENCY, 1);

/*==================
|   OPENGL         |
==================*/
    bool ANIMATE = false;
    int length = 800, height = 600;
	  double frameRate = 0.01;

	  Display* Frame = new Display("Hello World!", length, height);
	  Camera *Cam = new Camera(glm::vec3(-0.3,0.2,0.2), glm::vec3(0,0,0), glm::vec3(0,1,0));
	  Light *Lamp = new Light(glm::vec3(2,1,5));
	  Shader *myShader = new Shader("../graphic/vertex.glsl", "../graphic/fragment.glsl");
	  Model *Base = new Model("../graphic/","base");
	  Model *Robot = new Model("../graphic/","robobee");

    if (!ANIMATE){
      delete Frame;
      delete Cam;
      delete Lamp;
      delete myShader;
      delete Base;
      delete Robot;
    }

/*==================
|   RECORDING      |
==================*/
    // Create Saving Folder
    time_t timer = time(NULL);

    struct tm *tm_struct = localtime(&timer);

    char *fname;
    int dummy = asprintf(&fname, "Simulations/simtime_%d-%d-%d/", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);

    boost::filesystem::path dir(fname);
    if(boost::filesystem::create_directory(dir)) {
        std::cout << "Success" << "\n";
    }

    std::string folder(fname);

    double lenghtVecs = dynFreq*simt + 1;

    // Agent
    enum{VALUEFUN,POLICY,DOPA};
    arma::mat network(3, lenghtVecs);

    // Environment
    enum {REWARD,TDERROR};
    arma::mat environment(2, lenghtVecs);
    arma::mat state(q.size(), lenghtVecs),
              timeSim(lenghtVecs, 1),
              control(u.size(),lenghtVecs);

    // OpenGL frames
    // Frame->SetRecorder(folder + "flight.mp4");

/*========================================================================================================================*/



/*=====================================================RUNTIME PHASE======================================================*/
    // Create runtime object -> start runtime phase (end setup phase)
    MUSIC::Runtime *runtime = new MUSIC::Runtime(setup, TICK);

    // Initialize
    int iter = 0,
        prevStep = 0;

    double tickt = runtime->time(),
           loadDopa = 1.0,
           startSim = 2.0,
           clockStop = startSim,
           dynTime = 0,
           robotPos = 0,
           cageBound = std::pow(q_desired(8,0),2),
           omegaBound = 6*pi+pi/2,
           thetaBound = pi;

    bool netControl = false;

    arma::mat prevState(12,1, arma::fill::zeros),
              falseState(12,1, arma::fill::zeros);

    // Simulation Loop
    while (tickt < simt) {

        if (ANIMATE && std::abs(remainder(tickt,frameRate)) < 0.00001){
          Frame->Clear(0.0f, 0.1f, 0.15f, 1.0f);
			    myShader->Bind();
			    Robot->SetPos( glm::vec3( q(7,0), q(8,0), q(6,0) ) );
			    Robot->SetRot( glm::vec3( q(1,0), q(2,0), q(0,0) ) );
			    myShader->Update(*Robot, *Cam, *Lamp);
			    Robot->Draw();
			    myShader->Update(*Base, *Cam, *Lamp);
			    Base->Draw();
			    Frame->Update();
        }

        reward = 50*cos(state(0,int(tickt*dynFreq)));

        if(dynTime >= TICK && std::abs(remainder(dynTime,TICK)) < 0.00001)
        {
          prevStep = tickt/dynStep;
          prevState = state.col(prevStep);

          // Classical Controller calculates 3 control torques
          u = arma::join_vert(ctr.AltitudeControl(prevState), ctr.DampingControl(prevState));

          if (dynTime >= loadDopa)
            outhandler->SendState(prevState, tickt);

          // Dopaminergic Neurons Stimulation
          outhandler->SendReward(tdError, tickt);

          runtime->tick();  // Music Communication: spikes are sent and received here
          tickt = runtime->time();

          policy = inhandler->GetAction(tickt);       // Policy
          dopaActivity = inhandler->GetDopa(tickt);   // Dopaminergi neurons activity
          value = inhandler->GetValue(tickt, reward); // Value Function and TD-error
          valueFunction = value[0];
          tdError = value[1];

          if (netControl)
            u(1,0) = policy;

          if (clockStop >= 0)
            tdError = 0;
        }

        // Recording
        timeSim(iter,0) = dynTime;
        state.col(iter) = q;
        control.col(iter) = u;
        network(VALUEFUN, iter) = valueFunction;
        network(POLICY, iter) = policy;
        network(DOPA, iter) = dopaActivity;
        environment(REWARD, iter) = reward;
        environment(TDERROR, iter) = tdError;

        // Environment (RoboBee) generates the new state and reward
        q = bee.BeeDynamics(u);

        robotPos = std::pow(q(6,0),2)/4 + std::pow(q(7,0),2)/4 + std::pow(q(8,0)-q_desired(8,0),2);

        if (std::abs(q(0,0)) > thetaBound || std::abs(q(3,0)) > omegaBound || robotPos > cageBound)
          clockStop = 0.1;

        if (clockStop >= 0){
          q = q0;
          bee.SetState(q);
          clockStop-=dynStep;
        }

        dynTime += dynStep;

        iter++;
    }

    // End runtime phase
    runtime->finalize();
/*========================================================================================================================*/



/*=====================================================SAVING PHASE=======================================================*/


    state.save(folder + "state.dat",arma::raw_ascii); // arma::raw_ascii
    control.save(folder + "control.dat",arma::raw_ascii);
    timeSim.save(folder + "simtime.dat",arma::raw_ascii);
    network.save(folder + "network.dat",arma::raw_ascii);
    environment.save(folder + "environment.dat",arma::raw_ascii);

    arma::mat netParams;
    netParams.load("BeeBrain/matCritic_start.dat");
    netParams.save(folder + "matCritic_start.dat",arma::raw_ascii);
    netParams.clear();
    netParams.load("BeeBrain/matActor_start.dat");
    netParams.save(folder + "matActor_start.dat",arma::raw_ascii);
    netParams.clear();
    netParams.load("BeeBrain/matCritic_end.dat");
    netParams.save(folder + "matCritic_end.dat",arma::raw_ascii);
    netParams.clear();
    netParams.load("BeeBrain/matActor_end.dat");
    netParams.save(folder + "matActor_end.dat",arma::raw_ascii);

    Plotter Plot(folder);
    Plot.InState();
    Plot.RobotPos();
    Plot.NetActivity();
    Plot.EnvActivity();
    Plot.ValueMat(thetaBound, omegaBound);

/*========================================================================================================================*/



/*====================================================CLEANING PHASE======================================================*/

    delete runtime;
    delete inhandler;
    delete outhandler;


    // OpenGL
    if (ANIMATE){
      delete Frame;
      delete Cam;
      delete Lamp;
      delete myShader;
      delete Base;
      delete Robot;
    }

/*========================================================================================================================*/
}
// std::string loadfolder("BeeBrain");
// Iomanager manager(loadfolder, folder);
// std::vector <double> criticWeights = manager.ReadData("weightsCritic.out"),
//                      actorWeights = manager.ReadData("weightsActor.out");
// manager.PrintData("weightsCritic.dat", &criticWeights);
// manager.PrintData("weightsActor.dat", &actorWeights);
