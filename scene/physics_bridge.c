#include "physics_bridge.h"
#include <box3d/box3d.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>



typedef enum ColliderType
{
    COLLIDER_BOX,
    COLLIDER_SPHERE,
    COLLIDER_MESH,
    COLLIDER_CONVEX
} ColliderType;





struct B3PhysicsWorld;

typedef struct B3PhysicsBody
{
    struct B3PhysicsWorld* worldWrapper;
    b3WorldId worldId;
    b3BodyId bodyId;
    uint32_t entity_id;

    bool is_trigger;
    bool is_kinematic;
    
    ColliderType type;
    
    Vector3 base_extents;
    float base_radius;
    
    b3MeshData** mesh_data_array;
    int mesh_data_count;
    b3Vec3* mesh_vertices;
    int32_t* mesh_indices;
    
    b3HullData* convex_hull;
    
    Vector3 current_scale;
} B3PhysicsBody;





typedef struct B3PhysicsWorld
{
    b3WorldId worldId;
    B3PhysicsBody** bodies;
    int body_count;
    int body_capacity;
} B3PhysicsWorld;





typedef struct RaycastContext
{
    RaycastHit* hit;
    float max_distance;
    bool hit_triggers;
} RaycastContext;





typedef struct RaycastAllContext
{
    RaycastHit* hits;
    int max_hits;
    int hit_count;
    float max_distance;
    bool hit_triggers;
    b3Pos origin;
} RaycastAllContext;





// Custom callback for a raycast
static float PrismClosestRayCallback(b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId, int triangleIndex, int childIndex, void* context)
{
    (void)userMaterialId;
    (void)triangleIndex;
    (void)childIndex;
    RaycastContext* ctx = (RaycastContext*)context;
    if (!b3Shape_IsValid(shapeId))
        return -1.0f;

    B3PhysicsBody* w = (B3PhysicsBody*)b3Shape_GetUserData(shapeId);
    if (!w) return -1.0f;

    if (!ctx->hit_triggers && (w->is_trigger || b3Shape_IsSensor(shapeId)))
        return -1.0f;

    ctx->hit->hit = true;
    ctx->hit->entity_id = w->entity_id;
    ctx->hit->point = (Vector3){ point.x, point.y, point.z };
    ctx->hit->normal = (Vector3){ normal.x, normal.y, normal.z };
    ctx->hit->distance = fraction * ctx->max_distance;

    return fraction;
}





// Custom callback for a raycast that hits all meshes
static float PrismAllHitsRayCallback(b3ShapeId shapeId, b3Pos point, b3Vec3 normal, float fraction, uint64_t userMaterialId, int triangleIndex, int childIndex, void* context)
{
    (void)userMaterialId;
    (void)triangleIndex;
    (void)childIndex;
    RaycastAllContext* ctx = (RaycastAllContext*)context;
    if (!b3Shape_IsValid(shapeId))
        return 1.0f;

    B3PhysicsBody* w = (B3PhysicsBody*)b3Shape_GetUserData(shapeId);
    if (!w)
        return 1.0f;

    if (!ctx->hit_triggers && (w->is_trigger || b3Shape_IsSensor(shapeId)))
        return 1.0f;

    uint32_t current_id = w->entity_id;
    float current_dist = fraction * ctx->max_distance;

    bool already_found = false;
    for (int j = 0; j < ctx->hit_count; j++)
    {
        if (ctx->hits[j].entity_id == current_id)
        {
            already_found = true;
            if (current_dist < ctx->hits[j].distance)
            {
                ctx->hits[j].distance = current_dist;
                ctx->hits[j].point = (Vector3){ point.x, point.y, point.z };
                ctx->hits[j].normal = (Vector3){ normal.x, normal.y, normal.z };
            }
            
            break;
        }
    }

    if (!already_found && ctx->hit_count < ctx->max_hits)
    {
        ctx->hits[ctx->hit_count].hit = true;
        ctx->hits[ctx->hit_count].entity_id = current_id;
        ctx->hits[ctx->hit_count].distance = current_dist;
        ctx->hits[ctx->hit_count].point = (Vector3){ point.x, point.y, point.z };
        ctx->hits[ctx->hit_count].normal = (Vector3){ normal.x, normal.y, normal.z };
        ctx->hit_count++;
    }

    return 1.0f;
}





// Function that creates a B3PhysicsBody during object creation
static B3PhysicsBody* CreateBodyWrapper(B3PhysicsWorld* worldW, uint32_t entity_id, Vector3 position, bool is_trigger, ColliderType type)
{
    b3BodyDef bodyDef = b3DefaultBodyDef();
    bodyDef.type = b3_staticBody;
    bodyDef.position = (b3Pos){ position.x, position.y, position.z };

    B3PhysicsBody* wrapper = (B3PhysicsBody*)calloc(1, sizeof(B3PhysicsBody));
    if (!wrapper)
        return NULL;

    wrapper->worldWrapper = worldW;
    wrapper->worldId = worldW->worldId;
    wrapper->entity_id = entity_id;
    wrapper->is_trigger = is_trigger;
    wrapper->is_kinematic = false;
    wrapper->type = type;
    wrapper->mesh_data_array = NULL;
    wrapper->convex_hull = NULL;
    wrapper->current_scale = (Vector3){ 1.0f, 1.0f, 1.0f };

    bodyDef.userData = wrapper;
    wrapper->bodyId = b3CreateBody(worldW->worldId, &bodyDef);

    if (worldW->body_count >= worldW->body_capacity)
    {
        int new_capacity = worldW->body_capacity == 0 ? 32 : worldW->body_capacity * 2;
        B3PhysicsBody** new_bodies = (B3PhysicsBody**)realloc(worldW->bodies, sizeof(B3PhysicsBody*) * new_capacity);
        if (new_bodies)
        {
            worldW->bodies = new_bodies;
            worldW->body_capacity = new_capacity;
        }
    }
    if (worldW->body_count < worldW->body_capacity)
    {
        worldW->bodies[worldW->body_count++] = wrapper;
    }

    return wrapper;
}





// Used for creating shape definitions
static b3ShapeDef CreateShapeDef(B3PhysicsBody* wrapper, bool is_trigger)
{
    b3ShapeDef shapeDef = b3DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.isSensor = is_trigger;
    shapeDef.enableSensorEvents = true;
    if (wrapper->type == COLLIDER_MESH)
        shapeDef.enableSensorEvents = false; // Mesh shapes do not have proxies, disable sensor events
    shapeDef.enableContactEvents = !is_trigger;
    shapeDef.userData = wrapper;

    return shapeDef;
}





// Initializes a physics world
PhysicsWorldHandle Physics_InitWorld(void)
{
    b3WorldDef def = b3DefaultWorldDef();
    def.gravity = (b3Vec3){ 0.0f, -9.81f, 0.0f };
    b3WorldId worldId = b3CreateWorld(&def);

    B3PhysicsWorld* worldW = (B3PhysicsWorld*)calloc(1, sizeof(B3PhysicsWorld));
    if (!worldW)
        return NULL;

    worldW->worldId = worldId;
    worldW->bodies = NULL;
    worldW->body_count = 0;
    worldW->body_capacity = 0;

    return (PhysicsWorldHandle)worldW;
}





// Steps through the physics simulation by the specified delta time
void Physics_StepSimulation(PhysicsWorldHandle world, float delta_time)
{
    if (!world || delta_time <= 0.0f)
        return;
    
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;
    
    if (!b3World_IsValid(worldW->worldId))
        return;

    b3World_Step(worldW->worldId, delta_time, 4);
}





// Shuts down a physics world and deletes all the objects in it
void Physics_ShutdownWorld(PhysicsWorldHandle world)
{
    if (!world)
        return;
    
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;

    if (worldW->bodies)
    {
        for (int i = 0; i < worldW->body_count; i++)
        {
            B3PhysicsBody* bodyW = worldW->bodies[i];
            if (bodyW)
            {
                if (bodyW->mesh_data_array)
                {
                    for (int m = 0; m < bodyW->mesh_data_count; m++)
                    {
                        if (bodyW->mesh_data_array[m]) 
                            b3DestroyMesh(bodyW->mesh_data_array[m]);
                    }

                    free(bodyW->mesh_data_array);
                    bodyW->mesh_data_array = NULL;
                }
                if (bodyW->convex_hull)
                {
                    b3DestroyHull(bodyW->convex_hull);
                    bodyW->convex_hull = NULL;
                }

                free(bodyW);
            }
        }
        free(worldW->bodies);
        worldW->bodies = NULL;
    }
    worldW->body_count = 0;
    worldW->body_capacity = 0;

    if (b3World_IsValid(worldW->worldId))
        b3DestroyWorld(worldW->worldId);

    free(worldW);
}















// Creates a box collider with the specified extents
PhysicsBodyHandle Physics_CreateBoxCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, Vector3 extents, bool is_trigger)
{
    if (!world)
        return NULL;
    
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;
    B3PhysicsBody* wrapper = CreateBodyWrapper(worldW, entity_id, position, is_trigger, COLLIDER_BOX);
    
    if (!wrapper)
        return NULL;

    wrapper->base_extents = extents;

    b3ShapeDef shapeDef = CreateShapeDef(wrapper, is_trigger);
    b3BoxHull boxHull = b3MakeBoxHull(extents.x / 2.0f, extents.y / 2.0f, extents.z / 2.0f);
    b3CreateHullShape(wrapper->bodyId, &shapeDef, &boxHull.base);

    return (PhysicsBodyHandle)wrapper;
}





// Creates a sphere collider with the specified radius
PhysicsBodyHandle Physics_CreateSphereCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, float radius, bool is_trigger)
{
    if (!world)
        return NULL;
    
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;
    B3PhysicsBody* wrapper = CreateBodyWrapper(worldW, entity_id, position, is_trigger, COLLIDER_SPHERE);

    if (!wrapper)
        return NULL;

    wrapper->base_radius = radius;

    b3ShapeDef shapeDef = CreateShapeDef(wrapper, is_trigger);
    b3Sphere sphere = (b3Sphere){ (b3Vec3){ 0.0f, 0.0f, 0.0f }, radius };
    b3CreateSphereShape(wrapper->bodyId, &shapeDef, &sphere);

    return (PhysicsBodyHandle)wrapper;
}





// Creates a mesh collider with the specified vertices and indices
PhysicsBodyHandle Physics_CreateMeshCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, const void* vertices, int vertex_stride, int vertex_count, const uint32_t* indices, int index_count, bool is_trigger)
{
    if (!world || !vertices || !indices || vertex_count <= 0 || index_count <= 0)
        return NULL;
    
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;
    B3PhysicsBody* wrapper = CreateBodyWrapper(worldW, entity_id, position, is_trigger, COLLIDER_MESH);

    if (!wrapper)
        return NULL;

    b3Vec3* temp_vertices = (b3Vec3*)malloc(sizeof(b3Vec3) * vertex_count);
    int32_t* temp_indices = (int32_t*)malloc(sizeof(int32_t) * index_count);
    
    if (!temp_vertices || !temp_indices)
    {
        if (temp_vertices)
            free(temp_vertices);
        if (temp_indices)
            free(temp_indices);
        
        return (PhysicsBodyHandle)wrapper;
    }

    const uint8_t* ptr = (const uint8_t*)vertices;
    for (int i = 0; i < vertex_count; i++)
    {
        const float* f = (const float*)(ptr + i * vertex_stride);
        temp_vertices[i] = (b3Vec3){ f[0], f[1], f[2] };
    }

    for (int i = 0; i < index_count; i++)
    {
        temp_indices[i] = (int32_t)indices[i];
    }

    wrapper->mesh_vertices = temp_vertices;
    wrapper->mesh_indices = temp_indices;

    // Chunk meshes to bypass the 256 triangle limit
    int max_tris_per_chunk = 250;
    int total_tris = index_count / 3;
    int chunk_count = (total_tris + max_tris_per_chunk - 1) / max_tris_per_chunk;

    wrapper->mesh_data_array = (b3MeshData**)calloc(chunk_count, sizeof(b3MeshData*));
    wrapper->mesh_data_count = chunk_count;

    for (int c = 0; c < chunk_count; c++)
    {
        int tri_start = c * max_tris_per_chunk;
        int tris_in_chunk = max_tris_per_chunk;
        if (tri_start + tris_in_chunk > total_tris) 
            tris_in_chunk = total_tris - tri_start;

        int num_chunk_indices = tris_in_chunk * 3;
        b3Vec3* chunk_vertices = (b3Vec3*)malloc(sizeof(b3Vec3) * num_chunk_indices);
        int32_t* chunk_indices = (int32_t*)malloc(sizeof(int32_t) * num_chunk_indices);

        for (int i = 0; i < num_chunk_indices; i++)
        {
            int32_t original_index = temp_indices[tri_start * 3 + i];
            chunk_vertices[i] = temp_vertices[original_index];
            chunk_indices[i] = i; 
        }

        b3MeshDef meshDef = {0};
        meshDef.vertices = chunk_vertices;
        meshDef.vertexCount = num_chunk_indices;
        meshDef.indices = chunk_indices; 
        meshDef.triangleCount = tris_in_chunk;

        wrapper->mesh_data_array[c] = b3CreateMesh(&meshDef, NULL, 0);

        free(chunk_vertices);
        free(chunk_indices);

        b3ShapeDef shapeDef = CreateShapeDef(wrapper, is_trigger);
        if (wrapper->mesh_data_array[c])
            b3CreateMeshShape(wrapper->bodyId, &shapeDef, wrapper->mesh_data_array[c], (b3Vec3){ 1.0f, 1.0f, 1.0f });
    }

    return (PhysicsBodyHandle)wrapper;
}





// Creates a convex mesh collider with the specified vertices and indices
PhysicsBodyHandle Physics_CreateConvexCollider(PhysicsWorldHandle world, uint32_t entity_id, Vector3 position, const void* vertices, int vertex_stride, int vertex_count, bool is_trigger)
{
    if (!world || !vertices || vertex_count <= 0)
        return NULL;
    
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;
    B3PhysicsBody* wrapper = CreateBodyWrapper(worldW, entity_id, position, is_trigger, COLLIDER_CONVEX);

    if (!wrapper)
        return NULL;

    // Limit the maximum vertices for the generated convex hull to 64 to prevent hard crashes
    int max_hull_vertices = vertex_count;
    if (vertex_count > 64)
        max_hull_vertices = 64;

    b3Vec3* temp_vertices = (b3Vec3*)malloc(sizeof(b3Vec3) * max_hull_vertices);
    if (!temp_vertices)
        return (PhysicsBodyHandle)wrapper;

    const uint8_t* ptr = (const uint8_t*)vertices;
    for (int i = 0; i < max_hull_vertices; i++)
    {
        int index = (vertex_count > 64) ? (i * vertex_count) / max_hull_vertices : i;
        const float* f = (const float*)(ptr + index * vertex_stride);
        temp_vertices[i] = (b3Vec3){ f[0], f[1], f[2] };
    }

    wrapper->convex_hull = b3CreateHull(temp_vertices, max_hull_vertices, max_hull_vertices);
    wrapper->mesh_vertices = temp_vertices;

    b3ShapeDef shapeDef = CreateShapeDef(wrapper, is_trigger);
    if (wrapper->convex_hull)
        b3CreateHullShape(wrapper->bodyId, &shapeDef, wrapper->convex_hull);

    return (PhysicsBodyHandle)wrapper;
}





// Adds a rigidbody to a physics body
void Physics_AddRigidbody(PhysicsWorldHandle world, PhysicsBodyHandle body, float mass)
{
    (void)world;
    if (!body)
        return;
    B3PhysicsBody* w = (B3PhysicsBody*)body;
    
    if (!b3Body_IsValid(w->bodyId))
        return;

    if (mass > 0.0f)
    {
        b3Body_SetType(w->bodyId, b3_dynamicBody);
        b3Body_ApplyMassFromShapes(w->bodyId);
        b3MassData massData = b3Body_GetMassData(w->bodyId);

        if (massData.mass > 0.0f)
        {
            float scale = mass / massData.mass;
            massData.mass = mass;
            massData.inertia.cx.x *= scale; massData.inertia.cx.y *= scale; massData.inertia.cx.z *= scale;
            massData.inertia.cy.x *= scale; massData.inertia.cy.y *= scale; massData.inertia.cy.z *= scale;
            massData.inertia.cz.x *= scale; massData.inertia.cz.y *= scale; massData.inertia.cz.z *= scale;
            b3Body_SetMassData(w->bodyId, massData);
        }

        b3Body_SetAwake(w->bodyId, true);
    }
    else
    {
        b3Body_SetType(w->bodyId, b3_staticBody);
    }
}















// Returns a physics body position in the world
Vector3 Physics_GetBodyPosition(PhysicsBodyHandle body)
{
    if (!body)
        return (Vector3){0, 0, 0};

    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return (Vector3){0, 0, 0};

    b3Pos pos = b3Body_GetPosition(w->bodyId);
    return (Vector3){ pos.x, pos.y, pos.z };
}





// Returns a physics body rotation in the world as a Quaternion
Quaternion Physics_GetBodyRotation(PhysicsBodyHandle body)
{
    if (!body)
        return (Quaternion){0.0f, 0.0f, 0.0f, 1.0f};
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return (Quaternion){0.0f, 0.0f, 0.0f, 1.0f};

    b3Quat rot = b3Body_GetRotation(w->bodyId);
    return (Quaternion){ rot.v.x, rot.v.y, rot.v.z, rot.s };
}





// Returns a physics body scale in the world
Vector3 Physics_GetBodyScale(PhysicsBodyHandle body)
{
    if (!body)
        return (Vector3){0.0f, 0.0f, 0.0f};
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return (Vector3){0.0f, 0.0f, 0.0f};

    return w->current_scale;
}















// Sets the position of a physics body to a specified point
void Physics_SetBodyPosition(PhysicsBodyHandle body, Vector3 position)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    b3Pos current_p = b3Body_GetPosition(w->bodyId);
    if (fabsf(current_p.x - position.x) < 0.001f && 
        fabsf(current_p.y - position.y) < 0.001f && 
        fabsf(current_p.z - position.z) < 0.001f) 
    {
        return;
    }

    b3Pos p = (b3Pos){ position.x, position.y, position.z };
    b3Body_SetTransform(w->bodyId, p, b3Body_GetRotation(w->bodyId));
    b3Body_SetAwake(w->bodyId, true);
}





// Sets a physics body rotation to a specified quaternion
void Physics_SetBodyRotation(PhysicsBodyHandle body, Quaternion rotation)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    b3Quat q_curr = b3Body_GetRotation(w->bodyId);
    float dot = q_curr.v.x * rotation.x + q_curr.v.y * rotation.y + q_curr.v.z * rotation.z + q_curr.s * rotation.w;
    if (fabsf(dot) > 0.999f) 
        return;

    if (rotation.x == 0.0f && rotation.y == 0.0f && rotation.z == 0.0f && rotation.w == 0.0f)
        rotation.w = 1.0f;

    b3Quat q = (b3Quat){ (b3Vec3){ rotation.x, rotation.y, rotation.z }, rotation.w };
    b3Body_SetTransform(w->bodyId, b3Body_GetPosition(w->bodyId), q);
    b3Body_SetAwake(w->bodyId, true);
}





// Sets the scale of a physics body to a specified size
void Physics_SetBodyScale(PhysicsBodyHandle body, Vector3 scale)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    if (fabsf(w->current_scale.x - scale.x) < 0.001f &&
        fabsf(w->current_scale.y - scale.y) < 0.001f &&
        fabsf(w->current_scale.z - scale.z) < 0.001f) 
    {
        return; 
    }

    w->current_scale = scale;
    b3ShapeId shapeId;
    if (b3Body_GetShapes(w->bodyId, &shapeId, 1) != 1)
        return;

    if (w->type == COLLIDER_BOX)
    {
        Vector3 scaled_extents = (Vector3){ w->base_extents.x * scale.x, w->base_extents.y * scale.y, w->base_extents.z * scale.z };
        b3BoxHull boxHull = b3MakeBoxHull(scaled_extents.x / 2.0f, scaled_extents.y / 2.0f, scaled_extents.z / 2.0f);
        b3Shape_SetHull(shapeId, &boxHull.base);
        b3Body_ApplyMassFromShapes(w->bodyId);
    }
    else if (w->type == COLLIDER_SPHERE)
    {
        float max_s = scale.x > scale.y ? (scale.x > scale.z ? scale.x : scale.z) : (scale.y > scale.z ? scale.y : scale.z);
        b3Sphere sphere = b3Shape_GetSphere(shapeId);
        sphere.radius = w->base_radius * max_s;
        b3Shape_SetSphere(shapeId, &sphere);
        b3Body_ApplyMassFromShapes(w->bodyId);
    }
    else if (w->type == COLLIDER_MESH)
    {
        if (w->mesh_data_array)
        {
            // Allocate a temporary array to grab all shapes for this chunked mesh
            b3ShapeId* shapeIds = (b3ShapeId*)malloc(sizeof(b3ShapeId) * w->mesh_data_count);
            int count = b3Body_GetShapes(w->bodyId, shapeIds, w->mesh_data_count);

            for (int i = 0; i < count && i < w->mesh_data_count; i++)
                b3Shape_SetMesh(shapeIds[i], w->mesh_data_array[i], (b3Vec3){ scale.x, scale.y, scale.z });

            b3Body_ApplyMassFromShapes(w->bodyId);
            free(shapeIds);
        }
    }
    else if (w->type == COLLIDER_CONVEX)
    {
        if (w->convex_hull)
        {
            bool is_sensor = b3Shape_IsSensor(shapeId);
            b3Filter filter = b3Shape_GetFilter(shapeId);
            void* user_data = b3Shape_GetUserData(shapeId);

            b3ShapeDef shapeDef = b3DefaultShapeDef();
            shapeDef.density = 1.0f;
            shapeDef.isSensor = is_sensor;
            shapeDef.enableSensorEvents = is_sensor;
            shapeDef.enableContactEvents = true;
            shapeDef.filter = filter;
            shapeDef.userData = user_data;

            b3DestroyShape(shapeId, true);
            b3CreateTransformedHullShape(w->bodyId, &shapeDef, w->convex_hull, b3Transform_identity, (b3Vec3){ scale.x, scale.y, scale.z });
            b3Body_ApplyMassFromShapes(w->bodyId);
        }
    }

    b3Body_SetAwake(w->bodyId, true);
}





// Sets the linear velocity of a physics body
void Physics_SetLinearVelocity(PhysicsBodyHandle body, Vector3 velocity)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    b3Body_SetLinearVelocity(w->bodyId, (b3Vec3){ velocity.x, velocity.y, velocity.z });
    b3Body_SetAwake(w->bodyId, true);
}





// Sets the linear and angular damping variables of a physics body
void Physics_SetDamping(PhysicsBodyHandle body, float linear_drag, float angular_drag)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    b3Body_SetLinearDamping(w->bodyId, linear_drag);
    b3Body_SetAngularDamping(w->bodyId, angular_drag);
}





// Sets whether a physics body is affected by gravity
void Physics_SetGravityState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool use_gravity)
{
    (void)world;
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    b3Body_SetGravityScale(w->bodyId, use_gravity ? 1.0f : 0.0f);
    b3Body_SetAwake(w->bodyId, true);
}





// Sets the rotational constraints of a physics body
void Physics_SetRotationConstraints(PhysicsBodyHandle body, bool freeze_x, bool freeze_y, bool freeze_z)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    b3MotionLocks locks = b3Body_GetMotionLocks(w->bodyId);
    locks.angularX = freeze_x;
    locks.angularY = freeze_y;
    locks.angularZ = freeze_z;
    b3Body_SetMotionLocks(w->bodyId, locks);
}





// Sets whether a physics body is kinematic or dynamic
void Physics_SetKinematicState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool is_kinematic)
{
    (void)world;
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    w->is_kinematic = is_kinematic;
    if (is_kinematic)
    {
        b3Body_SetType(w->bodyId, b3_kinematicBody);
        b3Body_SetLinearVelocity(w->bodyId, (b3Vec3){ 0, 0, 0 });
        b3Body_SetAngularVelocity(w->bodyId, (b3Vec3){ 0, 0, 0 });
    }
    else
    {
        b3Body_SetType(w->bodyId, b3_dynamicBody);
        b3Body_SetAwake(w->bodyId, true);
    }
}





// Sets the box extents of a box collider
void Physics_SetBoxExtents(PhysicsBodyHandle body, Vector3 extents)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    w->base_extents = extents;
    b3ShapeId shapeId;
    if (b3Body_GetShapes(w->bodyId, &shapeId, 1) == 1)
    {
        Vector3 scaled_extents = (Vector3){ extents.x * w->current_scale.x, extents.y * w->current_scale.y, extents.z * w->current_scale.z };
        b3BoxHull boxHull = b3MakeBoxHull(scaled_extents.x / 2.0f, scaled_extents.y / 2.0f, scaled_extents.z / 2.0f);
        b3Shape_SetHull(shapeId, &boxHull.base);
        b3Body_ApplyMassFromShapes(w->bodyId);
        b3Body_SetAwake(w->bodyId, true);
    }
}





// Sets the radius of a sphere collider
void Physics_SetSphereRadius(PhysicsBodyHandle body, float radius)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    w->base_radius = radius;
    b3ShapeId shapeId;
    if (b3Body_GetShapes(w->bodyId, &shapeId, 1) == 1)
    {
        float max_s = w->current_scale.x > w->current_scale.y ? (w->current_scale.x > w->current_scale.z ? w->current_scale.x : w->current_scale.z) : (w->current_scale.y > w->current_scale.z ? w->current_scale.y : w->current_scale.z);
        b3Sphere sphere = b3Shape_GetSphere(shapeId);
        sphere.radius = radius * max_s;
        b3Shape_SetSphere(shapeId, &sphere);
        b3Body_ApplyMassFromShapes(w->bodyId);
        b3Body_SetAwake(w->bodyId, true);
    }
}





// Forces physics engine to recalculate the mass of a body
void Physics_RecalculateMass(PhysicsBodyHandle body, float mass)
{
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    if (mass > 0.0f)
    {
        if (b3Body_GetType(w->bodyId) != b3_dynamicBody)
            b3Body_SetType(w->bodyId, b3_dynamicBody);

        b3Body_ApplyMassFromShapes(w->bodyId);
        b3MassData massData = b3Body_GetMassData(w->bodyId);
        if (massData.mass > 0.0f)
        {
            float scale = mass / massData.mass;
            massData.mass = mass;
            massData.inertia.cx.x *= scale; massData.inertia.cx.y *= scale; massData.inertia.cx.z *= scale;
            massData.inertia.cy.x *= scale; massData.inertia.cy.y *= scale; massData.inertia.cy.z *= scale;
            massData.inertia.cz.x *= scale; massData.inertia.cz.y *= scale; massData.inertia.cz.z *= scale;
            b3Body_SetMassData(w->bodyId, massData);
        }
    }
    else
    {
        if (b3Body_GetType(w->bodyId) != b3_staticBody)
            b3Body_SetType(w->bodyId, b3_staticBody);
    }

    b3Body_SetAwake(w->bodyId, true);
}















// Sets the simulation state of a physics body
void Physics_SetBodySimulationState(PhysicsWorldHandle world, PhysicsBodyHandle body, bool enable_simulation)
{
    (void)world;
    if (!body)
        return;

    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    if (enable_simulation)
    {
        b3Body_Enable(w->bodyId);
        b3Body_SetAwake(w->bodyId, true);
    }
    else
    {
        b3Body_Disable(w->bodyId);
    }
}





// Returns all the collision events in a physics world
int Physics_GetEvents(PhysicsWorldHandle world, CollisionEvent* out_events, int max_events)
{
    if (!world || !out_events || max_events <= 0)
        return 0;
    
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;

    if (!b3World_IsValid(worldW->worldId))
        return 0;

    int event_count = 0;

    // --- Sensor Events (Triggers) ---
    b3SensorEvents sensorEvents = b3World_GetSensorEvents(worldW->worldId);
    
    for (int i = 0; i < sensorEvents.beginCount; i++)
    {
        if (event_count >= max_events)
            break;
        
        b3ShapeId sA = sensorEvents.beginEvents[i].sensorShapeId;
        b3ShapeId sB = sensorEvents.beginEvents[i].visitorShapeId;
        
        if (!b3Shape_IsValid(sA) || !b3Shape_IsValid(sB))
            continue;

        B3PhysicsBody* wA = (B3PhysicsBody*)b3Shape_GetUserData(sA);
        B3PhysicsBody* wB = (B3PhysicsBody*)b3Shape_GetUserData(sB);
        if (wA && wB)
            out_events[event_count++] = (CollisionEvent){ PHYS_EVENT_ENTER, wA->entity_id, wB->entity_id, true };
    }
    
    for (int i = 0; i < sensorEvents.endCount; i++)
    {
        if (event_count >= max_events)
            break;
        
        b3ShapeId sA = sensorEvents.endEvents[i].sensorShapeId;
        b3ShapeId sB = sensorEvents.endEvents[i].visitorShapeId;
        
        // If a shape was destroyed, IsValid returns false. ECS loop handles this cleanup
        if (!b3Shape_IsValid(sA) || !b3Shape_IsValid(sB))
            continue;

        B3PhysicsBody* wA = (B3PhysicsBody*)b3Shape_GetUserData(sA);
        B3PhysicsBody* wB = (B3PhysicsBody*)b3Shape_GetUserData(sB);
        if (wA && wB)
            out_events[event_count++] = (CollisionEvent){ PHYS_EVENT_EXIT, wA->entity_id, wB->entity_id, true };
    }

    // --- Contact Events (Solid Collisions) ---
    b3ContactEvents contactEvents = b3World_GetContactEvents(worldW->worldId);
    
    for (int i = 0; i < contactEvents.beginCount; i++)
    {
        if (event_count >= max_events)
            break;
        
        b3ShapeId sA = contactEvents.beginEvents[i].shapeIdA;
        b3ShapeId sB = contactEvents.beginEvents[i].shapeIdB;
        
        if (!b3Shape_IsValid(sA) || !b3Shape_IsValid(sB))
            continue;

        B3PhysicsBody* wA = (B3PhysicsBody*)b3Shape_GetUserData(sA);
        B3PhysicsBody* wB = (B3PhysicsBody*)b3Shape_GetUserData(sB);
        if (wA && wB)
            out_events[event_count++] = (CollisionEvent){ PHYS_EVENT_ENTER, wA->entity_id, wB->entity_id, false };
    }
    
    for (int i = 0; i < contactEvents.endCount; i++)
    {
        if (event_count >= max_events)
            break;
        
        b3ShapeId sA = contactEvents.endEvents[i].shapeIdA;
        b3ShapeId sB = contactEvents.endEvents[i].shapeIdB;
        
        if (!b3Shape_IsValid(sA) || !b3Shape_IsValid(sB))
            continue;

        B3PhysicsBody* wA = (B3PhysicsBody*)b3Shape_GetUserData(sA);
        B3PhysicsBody* wB = (B3PhysicsBody*)b3Shape_GetUserData(sB);
        if (wA && wB)
            out_events[event_count++] = (CollisionEvent){ PHYS_EVENT_EXIT, wA->entity_id, wB->entity_id, false };
    }

    return event_count;
}





// Sets the collision layer and mask of a physics body
void Physics_SetCollisionFilter(PhysicsWorldHandle world, PhysicsBodyHandle body, int layer, int mask)
{
    (void)world;
    if (!body)
        return;
    
    B3PhysicsBody* w = (B3PhysicsBody*)body;

    if (!b3Body_IsValid(w->bodyId))
        return;

    b3ShapeId shapeIds[16];
    int count = b3Body_GetShapes(w->bodyId, shapeIds, 16);
    for (int i = 0; i < count; i++)
    {
        if (!b3Shape_IsValid(shapeIds[i]))
            continue;
        
        b3Filter filter = b3Shape_GetFilter(shapeIds[i]);
        filter.categoryBits = (uint64_t)layer;
        filter.maskBits = (uint64_t)mask;
        b3Shape_SetFilter(shapeIds[i], filter, true);
    }
}















// Performs a raycast in the world with a given ray. Returns the first object hit
bool Physics_Raycast(PhysicsWorldHandle world, Ray ray, float max_distance, RaycastHit* out_hit, int collision_mask, bool hit_triggers)
{
    if (out_hit)
    {
        out_hit->hit = false;
        out_hit->entity_id = 0;
        out_hit->distance = 0.0f;
    }
    
    if (!world || !out_hit || max_distance <= 0.0f)
        return false;
    
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;
    if (!b3World_IsValid(worldW->worldId))
        return false;

    b3Pos origin = (b3Pos){ ray.origin.x, ray.origin.y, ray.origin.z };
    b3Vec3 translation = (b3Vec3){ ray.direction.x * max_distance, ray.direction.y * max_distance, ray.direction.z * max_distance };

    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = (uint64_t)-1;
    filter.maskBits = (uint64_t)collision_mask;

    RaycastContext ctx;
    ctx.hit = out_hit;
    ctx.max_distance = max_distance;
    ctx.hit_triggers = hit_triggers;

    b3World_CastRay(worldW->worldId, origin, translation, filter, PrismClosestRayCallback, &ctx);

    return out_hit->hit;
}





// Performs a raycast in the world with a given ray. Returns all objects hit
int Physics_RaycastAll(PhysicsWorldHandle world, Ray ray, float max_distance, RaycastHit* out_hits, int max_hits, int collision_mask, bool hit_triggers)
{
    if (!world || !out_hits || max_hits <= 0 || max_distance <= 0.0f)
        return 0;
    
    for (int i = 0; i < max_hits; i++)
    {
        out_hits[i].hit = false;
        out_hits[i].entity_id = 0;
        out_hits[i].distance = (float)INT_MAX;
    }
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;
    if (!b3World_IsValid(worldW->worldId))
        return 0;

    b3Pos origin = (b3Pos){ ray.origin.x, ray.origin.y, ray.origin.z };
    b3Vec3 translation = (b3Vec3){ ray.direction.x * max_distance, ray.direction.y * max_distance, ray.direction.z * max_distance };

    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = (uint64_t)-1;
    filter.maskBits = (uint64_t)collision_mask;

    RaycastAllContext ctx;
    ctx.hits = out_hits;
    ctx.max_hits = max_hits;
    ctx.hit_count = 0;
    ctx.max_distance = max_distance;
    ctx.hit_triggers = hit_triggers;
    ctx.origin = origin;

    b3World_CastRay(worldW->worldId, origin, translation, filter, PrismAllHitsRayCallback, &ctx);

    for (int i = 0; i < ctx.hit_count - 1; i++)
    {
        for (int j = 0; j < ctx.hit_count - i - 1; ++j)
        {
            if (out_hits[j].distance > out_hits[j + 1].distance)
            {
                RaycastHit temp = out_hits[j];
                out_hits[j] = out_hits[j + 1];
                out_hits[j + 1] = temp;
            }
        }
    }

    return ctx.hit_count;
}















// Destroys a physics body in a world
void Physics_DestroyBody(PhysicsWorldHandle world, PhysicsBodyHandle body)
{
    if (!body)
        return;

    B3PhysicsBody* bodyW = (B3PhysicsBody*)body;
    B3PhysicsWorld* worldW = (B3PhysicsWorld*)world;

    if (bodyW->worldWrapper)
        worldW = bodyW->worldWrapper;

    if (worldW && worldW->bodies)
    {
        for (int i = 0; i < worldW->body_count; i++)
        {
            if (worldW->bodies[i] == bodyW)
            {
                worldW->bodies[i] = worldW->bodies[worldW->body_count - 1];
                worldW->body_count--;
                break;
            }
        }
    }

    if (b3Body_IsValid(bodyW->bodyId))
        b3DestroyBody(bodyW->bodyId);
    if (bodyW->mesh_data_array)
    {
        for (int m = 0; m < bodyW->mesh_data_count; m++)
        {
            if (bodyW->mesh_data_array[m]) 
                b3DestroyMesh(bodyW->mesh_data_array[m]);
        }

        free(bodyW->mesh_data_array);
        bodyW->mesh_data_array = NULL;
    }
    if (bodyW->mesh_vertices)
        free(bodyW->mesh_vertices);
    if (bodyW->mesh_indices)
        free(bodyW->mesh_indices);
    if (bodyW->convex_hull)
    {
        b3DestroyHull(bodyW->convex_hull);
        bodyW->convex_hull = NULL;
    }
    
    free(bodyW);
}
