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



// World Management
PhysicsWorldHandle Physics_InitWorld(void);
void Physics_StepSimulation(PhysicsWorldHandle world, float delta_time);
void Physics_ShutdownWorld(PhysicsWorldHandle world);

// Body Creation
PhysicsBodyHandle Physics_CreateBoxCollider(PhysicsWorldHandle world, Vector3 position, Vector3 extents, bool is_trigger);
PhysicsBodyHandle Physics_CreateSphereCollider(PhysicsWorldHandle world, Vector3 position, float radius, bool is_trigger);
PhysicsBodyHandle Physics_CreateMeshCollider(PhysicsWorldHandle world, Vector3 position, const void* vertices, int vertex_stride, int vertex_count, const uint32_t* indices, int index_count, bool is_trigger);

void Physics_AddRigidbody(PhysicsWorldHandle world, PhysicsBodyHandle body, float mass);

// Data Retrieval
Vector3 Physics_GetBodyPosition(PhysicsBodyHandle body);
Quaternion Physics_GetBodyRotation(PhysicsBodyHandle body);

void Physics_SetBodyScale(PhysicsBodyHandle body, Vector3 scale);
void Physics_SetBodyPosition(PhysicsBodyHandle body, Vector3 position);
void Physics_SetBodyRotation(PhysicsBodyHandle body, Quaternion rotation);
void Physics_SetLinearVelocity(PhysicsBodyHandle body, Vector3 velocity);

void Physics_SetDamping(PhysicsBodyHandle body, float linear_drag, float angular_drag);
// void Physics_SetGravityState(PhysicsBodyHandle body, bool use_gravity);
void Physics_SetGravityState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool use_gravity);
void Physics_SetRotationConstraints(PhysicsBodyHandle body, bool freeze_x, bool freeze_y, bool freeze_z);
void Physics_SetKinematicState(PhysicsBodyHandle body, bool is_kinematic);

// Used for enabling/disabling entities
void Physics_SetBodySimulationState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool enable_simulation);


// Functions to delete physics entities
void Physics_DestroyBody(PhysicsWorldHandle world, PhysicsBodyHandle body);
void Physics_ShutdownWorld(PhysicsWorldHandle world);


#ifdef __cplusplus
}
#endif

#endif // PHYSICS_BRIDGE_H