
#include "KukaGraspExample.h"
#include "../SharedMemory/IKTrajectoryHelper.h"

#include "../CommonInterfaces/CommonGraphicsAppInterface.h"
#include "Bullet3Common/b3Quaternion.h"
#include "Bullet3Common/b3AlignedObjectArray.h"
#include "../CommonInterfaces/CommonRenderInterface.h"
#include "../CommonInterfaces/CommonExampleInterface.h"
#include "../CommonInterfaces/CommonGUIHelperInterface.h"
#include "../SharedMemory/PhysicsServerSharedMemory.h"
#include "../SharedMemory/PhysicsClientC_API.h"
#include <string>

#include "b3RobotSimAPI.h"
#include "../Utils/b3Clock.h"

///quick demo showing the right-handed coordinate system and positive rotations around each axis
class KukaGraspExample : public CommonExampleInterface
{
    CommonGraphicsApp* m_app;
	GUIHelperInterface* m_guiHelper;
	b3RobotSimAPI m_robotSim;
    int m_kukaIndex;
    
    IKTrajectoryHelper m_ikHelper;
    int m_targetSphereInstance;
    b3Vector3 m_targetPos;
    b3Vector3 m_worldPos;
    double m_time;
	int m_options;
	
	b3AlignedObjectArray<int> m_movingInstances;
	enum
	{
		numCubesX = 20,
		numCubesY = 20
	};
public:
    
    KukaGraspExample(GUIHelperInterface* helper, int options)
    :m_app(helper->getAppInterface()),
	m_guiHelper(helper),
	m_options(options),
	m_kukaIndex(-1),
    m_time(0)
    {
        m_targetPos.setValue(0.5,0,1);
        m_worldPos.setValue(0, 0, 0);
		m_app->setUpAxis(2);
    }
    virtual ~KukaGraspExample()
    {
        m_app->m_renderer->enableBlend(false);
    }

    
    virtual void physicsDebugDraw(int debugDrawMode)
    {
        
    }
    virtual void    initPhysics()
    {
        
        ///create some graphics proxy for the tracking target
        ///the endeffector tries to track it using Inverse Kinematics
        {
            int sphereId = m_app->registerGraphicsUnitSphereShape(SPHERE_LOD_MEDIUM);
            b3Quaternion orn(0, 0, 0, 1);
            b3Vector4 color = b3MakeVector4(1., 0.3, 0.3, 1);
            b3Vector3 scaling = b3MakeVector3(.02, .02, .02);
            m_targetSphereInstance = m_app->m_renderer->registerGraphicsInstance(sphereId, m_targetPos, orn, color, scaling);
        }
        m_app->m_renderer->writeTransforms();

        
        
        
        m_ikHelper.createKukaIIWA();
        
		bool connected = m_robotSim.connect(m_guiHelper);
		b3Printf("robotSim connected = %d",connected);
		
		
        {
            b3RobotSimLoadFileArgs args("");
            args.m_fileName = "kuka_iiwa/model.urdf";
            args.m_startPosition.setValue(0,0,0);
            b3RobotSimLoadFileResults results;
            if (m_robotSim.loadFile(args, results) && results.m_uniqueObjectIds.size()==1)
            {
                m_kukaIndex = results.m_uniqueObjectIds[0];
                int numJoints = m_robotSim.getNumJoints(m_kukaIndex);
                b3Printf("numJoints = %d",numJoints);

                for (int i=0;i<numJoints;i++)
                {
                    b3JointInfo jointInfo;
                    m_robotSim.getJointInfo(m_kukaIndex,i,&jointInfo);
                    b3Printf("joint[%d].m_jointName=%s",i,jointInfo.m_jointName);
                }
                /*
                int wheelJointIndices[4]={2,3,6,7};
                int wheelTargetVelocities[4]={-10,-10,-10,-10};
                for (int i=0;i<4;i++)
                {
                    b3JointMotorArgs controlArgs(CONTROL_MODE_VELOCITY);
                    controlArgs.m_targetVelocity = wheelTargetVelocities[i];
                    controlArgs.m_maxTorqueValue = 1e30;
                    m_robotSim.setJointMotorControl(m_kukaIndex,wheelJointIndices[i],controlArgs);
                }
                 */
            }
            
        	{
				b3RobotSimLoadFileArgs args("");
				args.m_fileName = "kiva_shelf/model.sdf";
                args.m_forceOverrideFixedBase = true;
				args.m_fileType = B3_SDF_FILE;
                args.m_startOrientation = b3Quaternion(0,0,0,1);
                b3RobotSimLoadFileResults results;
                m_robotSim.loadFile(args,results);
			}
			{
				b3RobotSimLoadFileArgs args("");
				args.m_fileName = "plane.urdf";
				args.m_startPosition.setValue(0,0,0);
				args.m_forceOverrideFixedBase = true;
				b3RobotSimLoadFileResults results;
				m_robotSim.loadFile(args,results);
				m_robotSim.setGravity(b3MakeVector3(0,0,0));
			}
	
		}
		
    }
    virtual void    exitPhysics()
    {
		m_robotSim.disconnect();
    }
    virtual void	stepSimulation(float deltaTime)
	{
		float dt = deltaTime;
		btClamp(dt,0.0001f,0.01f);

        m_time+=dt;
        m_targetPos.setValue(0.4-0.4*b3Cos( m_time), 0, 0.8+0.4*b3Cos( m_time));

        
        
        int numJoints = m_robotSim.getNumJoints(m_kukaIndex);
        
        if (numJoints==7)
        {
            double q_current[7]={0,0,0,0,0,0,0};

            double world_position[3]={0,0,0};
            b3JointStates jointStates;
            
            if (m_robotSim.getJointStates(m_kukaIndex,jointStates))
            {
                //skip the base positions (7 values)
                b3Assert(7+numJoints == jointStates.m_numDegreeOfFreedomQ);
                for (int i=0;i<numJoints;i++)
                {
                    q_current[i] = jointStates.m_actualStateQ[i+7];
                }
            }
            // compute body position
            m_robotSim.getLinkState(0, 6, world_position);
            m_worldPos.setValue(world_position[0], world_position[1], world_position[2]);
            
            b3Vector3DoubleData dataOut;
            m_targetPos.serializeDouble(dataOut);
            b3Vector3DoubleData worldPosDataOut;
            m_worldPos.serializeDouble(worldPosDataOut);

            
			b3RobotSimInverseKinematicArgs ikargs;
			b3RobotSimInverseKinematicsResults ikresults;
			
         
			ikargs.m_bodyUniqueId = m_kukaIndex;
//			ikargs.m_currentJointPositions = q_current;
//			ikargs.m_numPositions = 7;
            ikargs.m_endEffectorTargetPosition[0] = dataOut.m_floats[0];
            ikargs.m_endEffectorTargetPosition[1] = dataOut.m_floats[1];
            ikargs.m_endEffectorTargetPosition[2] = dataOut.m_floats[2];
			
//todo: orientation IK target
//			ikargs.m_endEffectorTargetOrientation[0] = 0;
//			ikargs.m_endEffectorTargetOrientation[1] = 0;
//			ikargs.m_endEffectorTargetOrientation[2] = 0;
//			ikargs.m_endEffectorTargetOrientation[3] = 1;

			if (m_robotSim.calculateInverseKinematics(ikargs,ikresults))
			{
                //copy the IK result to the desired state of the motor/actuator
                for (int i=0;i<numJoints;i++)
                {
                    b3JointMotorArgs t(CONTROL_MODE_POSITION_VELOCITY_PD);
                    t.m_targetPosition = ikresults.m_calculatedJointPositions[i];
                    t.m_maxTorqueValue = 1000;
                    t.m_kp= 1;
                    m_robotSim.setJointMotorControl(m_kukaIndex,i,t);

                }
            }
        }
        
        
		m_robotSim.stepSimulation();
    }
    virtual void	renderScene()
    {
		m_robotSim.renderScene();

        
        b3Quaternion orn(0, 0, 0, 1);
    
        
        m_app->m_renderer->writeSingleInstanceTransformToCPU(m_targetPos, orn, m_targetSphereInstance);
        m_app->m_renderer->writeTransforms();
        
        //draw the end-effector target sphere
        
		//m_app->m_renderer->renderScene();
    }

	
    virtual void	physicsDebugDraw()
    {
      	
    }
    virtual bool	mouseMoveCallback(float x,float y)
    {
		return false;   
    }
    virtual bool	mouseButtonCallback(int button, int state, float x, float y)
    {
        return false;   
    }
    virtual bool	keyboardCallback(int key, int state)
    {
        return false;   
    }
    
	virtual void resetCamera()
	{
		float dist = 3;
		float pitch = 0;
		float yaw = 30;
		float targetPos[3]={-0.2,0.8,0.3};
		if (m_app->m_renderer  && m_app->m_renderer->getActiveCamera())
		{
			m_app->m_renderer->getActiveCamera()->setCameraDistance(dist);
			m_app->m_renderer->getActiveCamera()->setCameraPitch(pitch);
			m_app->m_renderer->getActiveCamera()->setCameraYaw(yaw);
			m_app->m_renderer->getActiveCamera()->setCameraTargetPosition(targetPos[0],targetPos[1],targetPos[2]);
		}
	}

};


class	CommonExampleInterface*    KukaGraspExampleCreateFunc(struct CommonExampleOptions& options)
{
	return new KukaGraspExample(options.m_guiHelper, options.m_option);
}



