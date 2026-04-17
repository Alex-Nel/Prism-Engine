#ifndef PHYSICS_BRIDGE_H
#define PHYSICS_BRIDGE_H

#include <stdbool.h>
#include "../core/mathCore.h"

// --- Opaque Handles ---
typedef void* PhysicsWorldHandle;
typedef void* PhysicsBodyHandle;

#ifdef __cplusplus
extern "C" {
#endif



typedef struct RaycastHit
{
    bool has_hit;
    uint32_t entity_id; // The ID of the entity hit
    Vector3 point;      // The world coordinate of the impact
    Vector3 normal;     // The direction the hit surface is facing
    float distance;     // How far the ray traveled before hitting the object
} RaycastHit;



typedef struct CollisionPair
{
    uint32_t entity_a;
    uint32_t entity_b;
    bool is_trigger_event; // True if either object is a trigger
} CollisionPair;



// World Management
PhysicsWorldHandle Physics_InitWorld(void);
void Physics_StepSimulation(PhysicsWorldHandle world, float delta_time);
void Physics_ShutdownWorld(PhysicsWorldHandle world);

// Body Creation
PhysicsBodyHandle Physics_CreateBoxCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, Vector3 extents, bool is_trigger);
PhysicsBodyHandle Physics_CreateSphereCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, float radius, bool is_trigger);
PhysicsBodyHandle Physics_CreateMeshCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, const void* vertices, int vertex_stride, int vertex_count, const uint32_t* indices, int index_count, bool is_trigger);
void Physics_AddRigidbody(PhysicsWorldHandle world, PhysicsBodyHandle body, float mass);

// Data Retrieval
Vector3 Physics_GetBodyPosition(PhysicsBodyHandle body);
Quaternion Physics_GetBodyRotation(PhysicsBodyHandle body);

// Set physics properties
void Physics_SetBodyScale(PhysicsBodyHandle body, Vector3 scale);
void Physics_SetBodyPosition(PhysicsBodyHandle body, Vector3 position);
void Physics_SetBodyRotation(PhysicsBodyHandle body, Quaternion rotation);
void Physics_SetLinearVelocity(PhysicsBodyHandle body, Vector3 velocity);

void Physics_SetDamping(PhysicsBodyHandle body, float linear_drag, float angular_drag);
void Physics_SetGravityState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool use_gravity);
void Physics_SetRotationConstraints(PhysicsBodyHandle body, bool freeze_x, bool freeze_y, bool freeze_z);
void Physics_SetKinematicState(PhysicsBodyHandle body, bool is_kinematic);

// Used for enabling/disabling entities
void Physics_SetBodySimulationState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool enable_simulation);
int Physics_GetCollisions(PhysicsWorldHandle world, CollisionPair* out_pairs, int max_pairs);
void Physics_SetCollisionFilter(PhysicsWorldHandle world, PhysicsBodyHandle body, int layer, int mask);
bool Physics_Raycast(PhysicsWorldHandle world, Vector3 start, Vector3 end, RaycastHit* out_hit, int collision_mask);
int Physics_RaycastAll(PhysicsWorldHandle world, Vector3 start, Vector3 end, RaycastHit* out_hits, int max_hits, int collision_mask);


// Functions to delete physics entities
void Physics_DestroyBody(PhysicsWorldHandle world, PhysicsBodyHandle body);
void Physics_ShutdownWorld(PhysicsWorldHandle world);


#ifdef __cplusplus
}
#endif

#endif // PHYSICS_BRIDGE_H