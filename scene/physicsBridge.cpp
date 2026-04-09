#include <stdint.h>
#include "physicsBridge.h"
#include "../include/bullet/btBulletCollisionCommon.h"
#include "../include/bullet/btBulletDynamicsCommon.h"

extern "C"
{


// Initializes the physics world
PhysicsWorldHandle Physics_InitWorld(void)
{
    // Allocate Bullet's core C++ systems
    btDefaultCollisionConfiguration* config = new btDefaultCollisionConfiguration();
    btCollisionDispatcher* dispatcher = new btCollisionDispatcher(config);
    btBroadphaseInterface* broadphase = new btDbvtBroadphase();
    btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();

    // Create the world
    btDiscreteDynamicsWorld* world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, config);

    // Set Gravity (Standard Earth Gravity pointing down on the Y axis)
    world->setGravity(btVector3(0, -9.81f, 0));

    // Return it as an opaque C-pointer
    return (PhysicsWorldHandle)world;
}





// Step through the physics simulation in a specified delta time
void Physics_StepSimulation(PhysicsWorldHandle world, float delta_time)
{
    if (!world) return;
    
    // Cast the void* back into a Bullet object
    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;
    
    // step forward in time
    dynWorld->stepSimulation(delta_time, 10);
}





// Creates a Box Collider in the physics world
PhysicsBodyHandle Physics_CreateBoxCollider(PhysicsWorldHandle world, Vector3 position, Vector3 extents, bool is_trigger)
{
    if (!world) return nullptr;
    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;

    // Make the colliders shape using the extents
    btCollisionShape* shape = new btBoxShape(btVector3(extents.x, extents.y, extents.z));
    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(btVector3(position.x, position.y, position.z));

    // Always make static initially (Mass = 0)
    btDefaultMotionState* motionState = new btDefaultMotionState(trans);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, motionState, shape, btVector3(0,0,0));
    btRigidBody* body = new btRigidBody(rbInfo);

    // Sets collision flags
    if (is_trigger) body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    dynWorld->addRigidBody(body);

    return (PhysicsBodyHandle)body;
}





// Creates a Sphere Collider in the physics world
PhysicsBodyHandle Physics_CreateSphereCollider(PhysicsWorldHandle world, Vector3 position, float radius, bool is_trigger)
{
    if (!world) return nullptr;
    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;

    btCollisionShape* shape = new btSphereShape(radius);
    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(btVector3(position.x, position.y, position.z));

    btDefaultMotionState* motionState = new btDefaultMotionState(trans);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, motionState, shape, btVector3(0,0,0));
    btRigidBody* body = new btRigidBody(rbInfo);

    // Sets collision flags
    if (is_trigger) body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    dynWorld->addRigidBody(body);

    return (PhysicsBodyHandle)body;
}





// Creates a mesh collider in the physics world
PhysicsBodyHandle Physics_CreateMeshCollider(PhysicsWorldHandle world, Vector3 position, const void* vertices, int vertex_stride, int vertex_count, const uint32_t* indices, int index_count, bool is_trigger)
{
    if (!world) return nullptr;
    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;

    // Tell Bullet how to read the C structs
    btTriangleIndexVertexArray* meshInterface = new btTriangleIndexVertexArray(
        index_count / 3, (int*)indices, 3 * sizeof(uint32_t),
        vertex_count, (btScalar*)vertices, vertex_stride
    );

    // Create the BVH Tree Shape
    btBvhTriangleMeshShape* shape = new btBvhTriangleMeshShape(meshInterface, true);

    // Create physics transform
    btTransform trans;
    trans.setIdentity();
    trans.setOrigin(btVector3(position.x, position.y, position.z));

    // Meshes need to be static
    btDefaultMotionState* motionState = new btDefaultMotionState(trans);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, motionState, shape, btVector3(0,0,0));
    btRigidBody* body = new btRigidBody(rbInfo);

    // Sets collision flags
    if (is_trigger) body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
    dynWorld->addRigidBody(body);

    return (PhysicsBodyHandle)body;
}





// Adds a rigibody to the physics world
void Physics_AddRigidbody(PhysicsWorldHandle world, PhysicsBodyHandle body, float mass)
{
    if (!world || !body) return;

    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;
    btRigidBody* rb = (btRigidBody*)body;

    // Bullet needs to remove the body, change its mass, and re-add it 
    // so it moves from the "Static" lists to the "Dynamic" lists internally.
    dynWorld->removeRigidBody(rb);

    btVector3 inertia(0, 0, 0);
    if (mass > 0.0f)
        rb->getCollisionShape()->calculateLocalInertia(mass, inertia);

    // Reset collision flags
    rb->setCollisionFlags(rb->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
    
    rb->setMassProps(mass, inertia);
    rb->updateInertiaTensor();

    dynWorld->addRigidBody(rb);

    // If mass is over 0, activate rigidbody
    if (mass > 0.0f)
        rb->activate(true);
}





// Get an objects position in the physics world
Vector3 Physics_GetBodyPosition(PhysicsBodyHandle body)
{
    if (body == nullptr)
        return Vector3{0, 0, 0};
    
    btRigidBody* rb = (btRigidBody*)body;
    btTransform trans;
    
    // Bullet uses MotionStates to smoothly interpolate frames
    if (rb->getMotionState())
        rb->getMotionState()->getWorldTransform(trans);
    else
        trans = rb->getWorldTransform();

    return Vector3{trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ()};
}





// Get an objects rotation in the physics world
Quaternion Physics_GetBodyRotation(PhysicsBodyHandle body)
{
    if (!body)
        return Quaternion{0.0f, 0.0f, 0.0f, 1.0f}; // Assuming you have an identity quaternion fallback
    
    btRigidBody* rb = (btRigidBody*)body;
    btTransform trans;
    
    if (rb->getMotionState())
        rb->getMotionState()->getWorldTransform(trans);
    else
        trans = rb->getWorldTransform();

    // Extract the rotation
    btQuaternion q = trans.getRotation();
    
    return Quaternion{q.getX(), q.getY(), q.getZ(), q.getW()};
}





// Set an objects scale in the physics world
void Physics_SetBodyScale(PhysicsBodyHandle body, Vector3 scale)
{
    if (!body) return;
    btRigidBody* rb = (btRigidBody*)body;
    
    // Make the Bullet shape stretch to match the entities scale
    rb->getCollisionShape()->setLocalScaling(btVector3(scale.x, scale.y, scale.z));
}





// Set an objects position in the physics world
void Physics_SetBodyPosition(PhysicsBodyHandle body, Vector3 position)
{
    if (!body) return;
    btRigidBody* rb = (btRigidBody*)body;
    
    // Grab the current transform so it doesn't get overwritten
    btTransform trans = rb->getWorldTransform();
    
    // Update the position
    trans.setOrigin(btVector3(position.x, position.y, position.z));
    
    // Apply it back to the body and its motion state
    rb->setWorldTransform(trans);
    if (rb->getMotionState())
        rb->getMotionState()->setWorldTransform(trans);
    
    // Make sure the rigidbody is active
    rb->activate(true);
}





// Set an objects rotation in the physics world
void Physics_SetBodyRotation(PhysicsBodyHandle body, Quaternion rotation)
{
    if (!body) return;

    if (rotation.x == 0.0f && rotation.y == 0.0f && rotation.z == 0.0f && rotation.w == 0.0f)
        rotation.w = 1.0f; 
    
    btRigidBody* rb = (btRigidBody*)body;
    
    // Grab the current transform so it doesn't get overwritten
    btTransform trans = rb->getWorldTransform();
    
    // Update the rotation
    trans.setRotation(btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));
    
    // Apply it back to the body and its motion state
    rb->setWorldTransform(trans);
    if (rb->getMotionState())
        rb->getMotionState()->setWorldTransform(trans);
    
    // Make sure the rigidbody is active
    rb->activate(true);
}





// Set the linear velocity of a physics body
void Physics_SetLinearVelocity(PhysicsBodyHandle body, Vector3 velocity)
{
    if (!body) return;
    btRigidBody* rb = (btRigidBody*)body;
    
    // Activate the rigidbody
    rb->activate(true); 
    
    // Apply the movement force
    rb->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
}






// Set the damping of a physics obdy
void Physics_SetDamping(PhysicsBodyHandle body, float linear_drag, float angular_drag)
{
    if (!body) return;
    btRigidBody* rb = (btRigidBody*)body;
    
    // Bullet calls drag "Damping"
    rb->setDamping(linear_drag, angular_drag);
}





// Enabled or disabled the use of gravity on a physics body
void Physics_SetGravityState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool use_gravity)
{
    if (!world || !body) return;
    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;
    btRigidBody* rb = (btRigidBody*)body;
    
    if (use_gravity)
    {
        // Remove the disable flag
        rb->setFlags(rb->getFlags() & ~BT_DISABLE_WORLD_GRAVITY);
        
        // Give it the worlds gravity vector
        rb->setGravity(dynWorld->getGravity()); 
    }
    else
    {
        // Add the disable flag
        rb->setFlags(rb->getFlags() | BT_DISABLE_WORLD_GRAVITY);
        
        // Zero it out
        rb->setGravity(btVector3(0, 0, 0));
    }

    rb->activate(true);
}





// Set the rotation constraints of a rigidbody
void Physics_SetRotationConstraints(PhysicsBodyHandle body, bool freeze_x, bool freeze_y, bool freeze_z)
{
    if (!body) return;
    btRigidBody* rb = (btRigidBody*)body;
    
    // Bullet uses an "Angular Factor". 
    // 1.0 means it can spin freely. 0.0 means it is frozen on that axis.
    rb->setAngularFactor(btVector3(
        freeze_x ? 0.0f : 1.0f, 
        freeze_y ? 0.0f : 1.0f, 
        freeze_z ? 0.0f : 1.0f
    ));
}





// Sets a rigidbodies kinematic state to true or false 
void Physics_SetKinematicState(PhysicsBodyHandle body, bool is_kinematic)
{
    if (!body) return;

    btRigidBody* rb = (btRigidBody*)body;

    if (is_kinematic) 
    {
        // Set the Kinematic Flag
        rb->setCollisionFlags(rb->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        
        // Clear all momentum and forces
        rb->setLinearVelocity(btVector3(0, 0, 0));
        rb->setAngularVelocity(btVector3(0, 0, 0));
        rb->clearForces();
        
        // Stop physics from deactivation object
        rb->setActivationState(DISABLE_DEACTIVATION);
    } 
    else 
    {
        // Remove the Kinematic Flag
        rb->setCollisionFlags(rb->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        
        // Allow it to deactivate normally again when it stops moving
        rb->forceActivationState(ACTIVE_TAG);
        rb->activate(true);
    }
}





// Enable or disable a rigidbodies simulation state
void Physics_SetBodySimulationState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool enable_simulation)
{
    if (!world || !body) return;

    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;
    btRigidBody* rb = (btRigidBody*)body;

    if (enable_simulation) 
    {
        // Only add it if it isn't already in the world!
        if (!rb->isInWorld())
        {
            dynWorld->addRigidBody(rb);    
            rb->activate(true); // Enable it so that it starts simulating immediately
        }
    } 
    else 
    {
        // Completely remove it from the simulation
        if (rb->isInWorld())
            dynWorld->removeRigidBody(rb);
    }
}





// Remove a rigidbody from the physics world
void Physics_DestroyBody(PhysicsWorldHandle world, PhysicsBodyHandle body)
{
    if (!world || !body) return;

    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;
    btRigidBody* rb = (btRigidBody*)body;

    // Remove it from the active physics simulation
    if (rb->isInWorld())
        dynWorld->removeRigidBody(rb);

    // Delete the Motion State (what interpolates movement)
    if (rb->getMotionState())
        delete rb->getMotionState();

    // Delete the Collision Shape
    btCollisionShape* shape = rb->getCollisionShape();
    if (shape)
    {
        // If this is a complex mesh, we must also delete the Vertex Array
        if (shape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
        {
            btBvhTriangleMeshShape* meshShape = (btBvhTriangleMeshShape*)shape;
            if (meshShape->getMeshInterface())
                delete meshShape->getMeshInterface();
        }
        
        delete shape;
    }

    // Delete the RigidBody itself
    delete rb;
}





// Shuts down the physics world
void Physics_ShutdownWorld(PhysicsWorldHandle world)
{
    if (!world) return;
    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;

    // Delete things in the reverse order that they were created
    btConstraintSolver* solver = dynWorld->getConstraintSolver();
    btBroadphaseInterface* broadphase = dynWorld->getBroadphase();
    btDispatcher* dispatcher = dynWorld->getDispatcher();
    btCollisionDispatcher* collDispatcher = (btCollisionDispatcher*)dispatcher;
    btCollisionConfiguration* collisionConfig = collDispatcher ? collDispatcher->getCollisionConfiguration() : nullptr;

    // Delete the World
    delete dynWorld;
    
    // Delete the sub-systems
    delete solver;
    delete broadphase;
    delete dispatcher;
    delete collisionConfig;
}


}