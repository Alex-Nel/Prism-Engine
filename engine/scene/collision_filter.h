#ifndef COLLISION_FILTER_H
#define COLLISION_FILTER_H



// A collider belongs to exactly one collision layer.
typedef enum CollisionLayer
{
    COLLISION_LAYER_NONE      = 0,
    COLLISION_LAYER_DEFAULT   = (1 << 0),
    COLLISION_LAYER_TRIGGER   = (1 << 1),
    COLLISION_LAYER_USER_1    = (1 << 2),
    COLLISION_LAYER_USER_2    = (1 << 3),
    COLLISION_LAYER_USER_3    = (1 << 4),
    COLLISION_LAYER_USER_4    = (1 << 5),
    COLLISION_LAYER_USER_5    = (1 << 6),
    COLLISION_LAYER_USER_6    = (1 << 7),
    COLLISION_LAYER_USER_7    = (1 << 8),
    COLLISION_LAYER_USER_8    = (1 << 9),
    COLLISION_LAYER_USER_9    = (1 << 10),
    COLLISION_LAYER_USER_10   = (1 << 11),
    COLLISION_LAYER_USER_11   = (1 << 12),
    COLLISION_LAYER_USER_12   = (1 << 13),
    COLLISION_LAYER_USER_13   = (1 << 14)
} CollisionLayer;



// A collision mask is a set of layers. Values may be combined with bitwise OR.
typedef enum CollisionMask
{
    COLLISION_MASK_NONE      = 0,
    COLLISION_MASK_DEFAULT   = (1 << 0),
    COLLISION_MASK_TRIGGER   = (1 << 1),
    COLLISION_MASK_USER_1    = (1 << 2),
    COLLISION_MASK_USER_2    = (1 << 3),
    COLLISION_MASK_USER_3    = (1 << 4),
    COLLISION_MASK_USER_4    = (1 << 5),
    COLLISION_MASK_USER_5    = (1 << 6),
    COLLISION_MASK_USER_6    = (1 << 7),
    COLLISION_MASK_USER_7    = (1 << 8),
    COLLISION_MASK_USER_8    = (1 << 9),
    COLLISION_MASK_USER_9    = (1 << 10),
    COLLISION_MASK_USER_10   = (1 << 11),
    COLLISION_MASK_USER_11   = (1 << 12),
    COLLISION_MASK_USER_12   = (1 << 13),
    COLLISION_MASK_USER_13   = (1 << 14),
    COLLISION_MASK_ALL       = -1
} CollisionMask;



#endif // COLLISION_FILTER_H