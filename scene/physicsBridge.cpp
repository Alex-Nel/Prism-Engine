#include <stdint.h>
#include "physicsBridge.h"
// #include "../include/bullet/btBulletCollisionCommon.h"
#include <btBulletCollisionCommon.h>
// #include "../include/bullet/btBulletDynamicsCommon.h"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>


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
PhysicsBodyHandle Physics_CreateBoxCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, Vector3 extents, bool is_trigger)
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

    body->setUserIndex((int)entity_id);

    dynWorld->addRigidBody(body);

    return (PhysicsBodyHandle)body;
}





// Creates a Sphere Collider in the physics world
PhysicsBodyHandle Physics_CreateSphereCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, float radius, bool is_trigger)
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

    body->setUserIndex((int)entity_id);

    dynWorld->addRigidBody(body);

    return (PhysicsBodyHandle)body;
}





// Creates a mesh collider in the physics world
PhysicsBodyHandle Physics_CreateMeshCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, const void* vertices, int vertex_stride, int vertex_count, const uint32_t* indices, int index_count, bool is_trigger)
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

    body->setUserIndex((int)entity_id);

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





// Gets all collision pairs that happened in a frame
int Physics_GetCollisions(PhysicsWorldHandle world, CollisionPair* out_pairs, int max_pairs)
{
    if (!world || !out_pairs) return 0;
    
    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;
    
    int pair_count = 0;
    int numManifolds = dynWorld->getDispatcher()->getNumManifolds();
    
    for (int i = 0; i < numManifolds; i++)
    {
        if (pair_count >= max_pairs) break; // Prevents overflowing

        btPersistentManifold* contactManifold = dynWorld->getDispatcher()->getManifoldByIndexInternal(i);
        
        // If there are 0 contacts, they aren't actually touching
        if (contactManifold->getNumContacts() > 0)
        {
            const btCollisionObject* obA = contactManifold->getBody0();
            const btCollisionObject* obB = contactManifold->getBody1();
            
            CollisionPair pair;
            pair.entity_a = (uint32_t)obA->getUserIndex();
            pair.entity_b = (uint32_t)obB->getUserIndex();
            
            // Check if either object is flagged as a trigger (NO_CONTACT_RESPONSE)
            bool a_is_trigger = obA->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE;
            bool b_is_trigger = obB->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE;
            
            pair.is_trigger_event = (a_is_trigger || b_is_trigger);
            
            out_pairs[pair_count] = pair;
            pair_count++;
        }
    }
    
    return pair_count;
}





// Sets the collision filter and mask of a physics body
void Physics_SetCollisionFilter(PhysicsWorldHandle world, PhysicsBodyHandle body, int layer, int mask)
{
    if (!world || !body) return;

    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;
    btRigidBody* rb = (btRigidBody*)body;

    // Bullet requires removing the body to update its broadphase filtering
    dynWorld->removeRigidBody(rb);
    dynWorld->addRigidBody(rb, layer, mask);
}





// Does a raycast at a start and end point, returns first hit entity
bool Physics_Raycast(PhysicsWorldHandle world, Vector3 start, Vector3 end, RaycastHit* out_hit, int collision_mask)
{
    // zero out the hit struct first
    if (out_hit)
    {
        out_hit->has_hit = false;
        out_hit->entity_id = 0;
        out_hit->distance = 0.0f;
    }

    if (!world || !out_hit) return false;
    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;

    // Convert engine Vectors to Bullet Vectors
    btVector3 btStart(start.x, start.y, start.z);
    btVector3 btEnd(end.x, end.y, end.z);

    // Create the Bullet Raycast Callback
    btCollisionWorld::ClosestRayResultCallback rayCallback(btStart, btEnd);

    rayCallback.m_collisionFilterGroup = -1;              // Ray is in "All layers"
    rayCallback.m_collisionFilterMask = collision_mask;   // Determines what the ray is allowed to hit
    
    // Perform raycast
    dynWorld->rayTest(btStart, btEnd, rayCallback);

    // If raycast hit something, save information
    if (rayCallback.hasHit()) 
    {
        out_hit->has_hit = true;
        
        // Get the Entity ID
        if (rayCallback.m_collisionObject)
            out_hit->entity_id = (uint32_t)rayCallback.m_collisionObject->getUserIndex();

        // Save the exact impact point
        out_hit->point = Vector3{
            rayCallback.m_hitPointWorld.x(), 
            rayCallback.m_hitPointWorld.y(), 
            rayCallback.m_hitPointWorld.z()
        };

        // Get the surface normal
        out_hit->normal = Vector3{
            rayCallback.m_hitNormalWorld.x(), 
            rayCallback.m_hitNormalWorld.y(), 
            rayCallback.m_hitNormalWorld.z()
        };

        // Calculate the distance traveled
        out_hit->distance = (btStart - rayCallback.m_hitPointWorld).length();

        return true;
    }

    return false;
}





// Performs a raycast and collects multiple entities hit
int Physics_RaycastAll(PhysicsWorldHandle world, Vector3 start, Vector3 end, RaycastHit* out_hits, int max_hits, int collision_mask)
{
    if (!world || !out_hits || max_hits <= 0) return 0;

    // Zero out the array of RaycastHits
    for (int i = 0; i < max_hits; i++)
    {
        out_hits[i].has_hit = false;
        out_hits[i].entity_id = 0;
        out_hits[i].distance = INT32_MAX; // Set very high for comparisons
    }

    btDiscreteDynamicsWorld* dynWorld = (btDiscreteDynamicsWorld*)world;

    btVector3 btStart(start.x, start.y, start.z);
    btVector3 btEnd(end.x, end.y, end.z);

    // Use the "All Hits" callback instead of the "Closest" callback
    btCollisionWorld::AllHitsRayResultCallback rayCallback(btStart, btEnd);

    rayCallback.m_collisionFilterGroup = -1;              // Ray is in "All layers"
    rayCallback.m_collisionFilterMask = collision_mask;   // Determines what the ray is allowed to hit

    dynWorld->rayTest(btStart, btEnd, rayCallback);

    int unique_hit_count = 0;

    // Collect all the hits from Bullet's internal arrays
    for (int i = 0; i < rayCallback.m_collisionObjects.size(); i++)
    {
        if (!rayCallback.m_collisionObjects[i]) continue;
        
        uint32_t current_id = (uint32_t)rayCallback.m_collisionObjects[i]->getUserIndex();
        float current_dist = (btStart - rayCallback.m_hitPointWorld[i]).length();

        // Check if we already registered a hit for this exact Entity ID
        bool already_found = false;
        for (int j = 0; j < unique_hit_count; j++) 
        {
            if (out_hits[j].entity_id == current_id) 
            {
                already_found = true;
                
                // If this specific triangle is CLOSER than the last triangle we hit on this mesh, overwrite it!
                if (current_dist < out_hits[j].distance)
                {
                    out_hits[j].distance = current_dist;
                    out_hits[j].point = Vector3{ rayCallback.m_hitPointWorld[i].x(), rayCallback.m_hitPointWorld[i].y(), rayCallback.m_hitPointWorld[i].z() };
                    out_hits[j].normal = Vector3{ rayCallback.m_hitNormalWorld[i].x(), rayCallback.m_hitNormalWorld[i].y(), rayCallback.m_hitNormalWorld[i].z() };
                }

                break;
            }
        }

        // If it's a new entity, add it to our array (if we have space)
        if (!already_found && unique_hit_count < max_hits) 
        {
            out_hits[unique_hit_count].has_hit = true;
            out_hits[unique_hit_count].entity_id = current_id;
            out_hits[unique_hit_count].distance = current_dist;
            out_hits[unique_hit_count].point = Vector3{ rayCallback.m_hitPointWorld[i].x(), rayCallback.m_hitPointWorld[i].y(), rayCallback.m_hitPointWorld[i].z() };
            out_hits[unique_hit_count].normal = Vector3{ rayCallback.m_hitNormalWorld[i].x(), rayCallback.m_hitNormalWorld[i].y(), rayCallback.m_hitNormalWorld[i].z() };
            
            unique_hit_count++;
        }
    }


    // Sort the unique hits from closest to furthest (Bubble sort for now)
    for (int i = 0; i < unique_hit_count - 1; i++) 
    {
        for (int j = 0; j < unique_hit_count - i - 1; j++) 
        {
            if (out_hits[j].distance > out_hits[j + 1].distance) 
            {
                RaycastHit temp = out_hits[j];
                out_hits[j] = out_hits[j + 1];
                out_hits[j + 1] = temp;
            }
        }
    }

    return unique_hit_count;
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