#include "material_loader.h"
#include "asset_manager.h"
#include "core/io_core.h"
#include "core/log_core.h"
#include "core/image_core.h"

#include <cJSON/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <limits.h>
#endif





static char* DuplicateString(const char* s)
{
    if (!s)
    	return NULL;

    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    
    if (copy)
        memcpy(copy, s, len);
    
    return copy;
}










static bool IsAbsolutePath(const char* path)
{
    if (!path || path[0] == '\0')
    	return false;

    if (path[0] == '/' || path[0] == '\\')
    	return true;

    if (isalpha((unsigned char)path[0]) && path[1] == ':')
    	return true;

    return false;
}










static const char* GetBaseFilename(const char* path)
{
    if (!path)
    	return "";

    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    const char* split = (last_slash > last_backslash) ? last_slash : last_backslash;
    
    return split ? split + 1 : path;
}










static void GetFilenameStem(const char* filepath, char* out_stem, size_t out_size)
{
    if (!filepath || !out_stem || out_size == 0)
    	return;

    const char* base = GetBaseFilename(filepath);
    snprintf(out_stem, out_size, "%s", base);
    char* dot = strrchr(out_stem, '.');
    
    if (dot)
    	*dot = '\0';
}










static void GetDirectoryPath(const char* filepath, char* out_dir, size_t out_size)
{
    if (!filepath || !out_dir || out_size == 0)
    	return;

    snprintf(out_dir, out_size, "%s", filepath);
    char* last_slash = strrchr(out_dir, '/');
    char* last_backslash = strrchr(out_dir, '\\');
    char* split = (last_slash > last_backslash) ? last_slash : last_backslash;
    
    if (split)
        *(split + 1) = '\0';
    else
        out_dir[0] = '\0';
}










static bool FileExists(const char* path)
{
    if (!path || path[0] == '\0')
    	return false;

    FILE* f = fopen(path, "rb");
    if (f)
    {
        fclose(f);
        return true;
    }
    
    return false;
}










static bool NormalizeAndCanonicalize(const char* path, char* out, size_t out_size)
{
    if (!path || !out || out_size == 0)
    	return false;

#ifdef _WIN32
    char temp[1024];
    
    if (_fullpath(temp, path, sizeof(temp)) == NULL)
        return false;

    for (char* p = temp; *p; p++)
    {
        if (*p == '\\') *p = '/';
    }
    
    snprintf(out, out_size, "%s", temp);
    return true;
#else
    char temp[PATH_MAX];
    
    if (realpath(path, temp) == NULL)
    {
        return false;
    }
    
    snprintf(out, out_size, "%s", temp);
    return true;
#endif
}










static bool PathsEqual(const char* a, const char* b)
{
    if (!a || !b)
    	return false;

#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}










static bool HasDuplicateKeys(const cJSON* item)
{
    if (!item)
    	return false;

    if (cJSON_IsObject(item))
    {
        for (const cJSON* a = item->child; a; a = a->next)
        {
            for (const cJSON* b = a->next; b; b = b->next)
            {
                if (a->string && b->string && strcmp(a->string, b->string) == 0)
                    return true;
            }

            if (HasDuplicateKeys(a))
                return true;
        }
    }
    else if (cJSON_IsArray(item))
    {
        for (const cJSON* a = item->child; a; a = a->next)
        {
            if (HasDuplicateKeys(a))
                return true;
        }
    }

    return false;
}










static Texture* ResolveSolidColorTexture(AssetManager* am, Color color, bool* out_is_new)
{
    *out_is_new = false;
    unsigned char r_b = (unsigned char)(color.r * 255.0f);
    unsigned char g_b = (unsigned char)(color.g * 255.0f);
    unsigned char b_b = (unsigned char)(color.b * 255.0f);
    unsigned char a_b = (unsigned char)(color.a * 255.0f);

    char name[64];
    snprintf(name, sizeof(name), "SolidColor_%02x%02x%02x%02x", r_b, g_b, b_b, a_b);
    
    for (uint32_t i = 0; i < am->texture_count; i++)
    {
        if (am->texture_cache[i].name[0] != '\0' && strcmp(am->texture_cache[i].name, name) == 0)
        {
            return &am->texture_cache[i];
        }
    }

    Texture* t = Asset_CreateSolidColorTexture(am, name, color);
    if (!t || t->gpu_handle.id == 0)
    {
        if (t)
        {
            Asset_UnloadTexture(am, t);
        }
        
        return NULL;
    }

    *out_is_new = true;
    return t;
}










static Texture* ResolveFileTexture(AssetManager* am, const char* mat_dir, const char* rel_path, bool* out_is_new)
{
    *out_is_new = false;
    char combined[1024];
    snprintf(combined, sizeof(combined), "%s%s", mat_dir, rel_path);

    char canonical[1024];
    if (!NormalizeAndCanonicalize(combined, canonical, sizeof(canonical)))
        return NULL;
    
    if (!FileExists(canonical))
        return NULL;
    
    // Check if texture is already loaded with this canonical path
    for (uint32_t i = 0; i < am->texture_count; i++)
    {
        if (am->texture_cache[i].name[0] != '\0' && am->texture_source_paths[i])
        {
            if (PathsEqual(am->texture_source_paths[i], canonical))
            {
                return &am->texture_cache[i];
            }
        }
    }

    // Check if texture pool has space
    bool has_slot = false;
    for (uint32_t i = 0; i < am->texture_count; i++)
    {
        if (am->texture_cache[i].name[0] == '\0')
        {
            has_slot = true;
            break;
        }
    }

    if (!has_slot && am->texture_count >= MAX_CACHED_TEXTURES)
        return NULL;
    
    const char* base_name = GetBaseFilename(rel_path);
    Texture* t = Asset_LoadTexture(am, base_name, canonical);
    
    if (!t || t->gpu_handle.id == 0)
    {
        if (t)
        {
            Asset_UnloadTexture(am, t);
        }
        return NULL;
    }
    
    if (am->texture_source_paths[t->id])
        free(am->texture_source_paths[t->id]);
    am->texture_source_paths[t->id] = DuplicateString(canonical);

    *out_is_new = true;
    return t;
}










static Texture* ResolveTextureSlot(AssetManager* am, cJSON* slot_item, const char* mat_dir, bool is_albedo, bool* out_is_new)
{
    *out_is_new = false;
    if (!slot_item || cJSON_IsNull(slot_item))
    {
        if (is_albedo)
            return Asset_GetDefaultTexture(am);
        return NULL;
    }

    if (cJSON_IsString(slot_item))
    {
        return ResolveFileTexture(am, mat_dir, slot_item->valuestring, out_is_new);
    }
    else if (cJSON_IsObject(slot_item))
    {
        cJSON* col_arr = cJSON_GetObjectItemCaseSensitive(slot_item, "color");
        if (!col_arr || !cJSON_IsArray(col_arr))
            return NULL;

        int size = cJSON_GetArraySize(col_arr);
        if (size < 3 || size > 4)
            return NULL;
        
        float c[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        for (int i = 0; i < size; i++)
        {
            cJSON* el = cJSON_GetArrayItem(col_arr, i);
            c[i] = (float)el->valuedouble;
        }
        
        Color color = {c[0], c[1], c[2], c[3]};
        return ResolveSolidColorTexture(am, color, out_is_new);
    }

    return NULL;
}










Material* MaterialLoader_Load(AssetManager* am, const char* filepath)
{
    if (!am || !filepath || filepath[0] == '\0')
        return NULL;

    // Check if file exists
    if (!FileExists(filepath))
        return NULL;
    
    char canonical_mat_path[1024];
    if (!NormalizeAndCanonicalize(filepath, canonical_mat_path, sizeof(canonical_mat_path)))
        return NULL;
    
    // Check if this material is already loaded
    for (uint32_t i = 0; i < am->material_count; i++)
    {
        if (am->material_cache[i].active && am->material_source_paths[i])
        {
            if (PathsEqual(am->material_source_paths[i], canonical_mat_path))
            {
                return &am->material_cache[i];
            }
        }
    }

    // Check if material pool has space BEFORE allocating any dependencies
    bool has_material_slot = false;
    for (uint32_t i = 0; i < am->material_count; i++)
    {
        if (!am->material_cache[i].active)
        {
            has_material_slot = true;
            break;
        }
    }

    if (!has_material_slot && am->material_count >= MAX_MATERIALS)
    {
        Log_Error("ERROR: Material pool full\n");
        return NULL;
    }
    
    // Read file
    char* buffer = IO_ReadTextFile(filepath);
    if (!buffer)
        return NULL;

    // Parse JSON and check trailing content
    const char* parse_end = NULL;
    cJSON* root = cJSON_ParseWithOpts(buffer, &parse_end, 0);
    if (!root)
    {
        free(buffer);
        return NULL;
    }

    if (parse_end)
    {
        while (*parse_end != '\0')
        {
            if (!isspace((unsigned char)*parse_end))
            {
                cJSON_Delete(root);
                free(buffer);
                return NULL;
            }
            parse_end++;
        }
    }
    
    free(buffer);
    
    if (!cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return NULL;
    }

    if (HasDuplicateKeys(root))
    {
        cJSON_Delete(root);
        return NULL;
    }

    // Validate top-level keys strictly
    for (const cJSON* child = root->child; child; child = child->next)
    {
        if (!child->string)
        {
            cJSON_Delete(root);
            return NULL;
        }
        if (strcmp(child->string, "version") != 0 &&
            strcmp(child->string, "name") != 0 &&
            strcmp(child->string, "shader") != 0 &&
            strcmp(child->string, "properties") != 0 &&
            strcmp(child->string, "textures") != 0)
        {
            cJSON_Delete(root);
            return NULL;
        }
    }

    // Version validation (must be integer 1)
    cJSON* ver_item = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (!ver_item || !cJSON_IsNumber(ver_item))
    {
        cJSON_Delete(root);
        return NULL;
    }
    
    if (ver_item->valuedouble != (double)ver_item->valueint || ver_item->valueint != 1)
    {
        cJSON_Delete(root);
        return NULL;
    }

    // Name validation
    char mat_name[MAX_NAME_LENGTH] = {0};
    cJSON* name_item = cJSON_GetObjectItemCaseSensitive(root, "name");
    
    if (name_item)
    {
        if (!cJSON_IsString(name_item) || name_item->valuestring[0] == '\0')
        {
            cJSON_Delete(root);
            return NULL;
        }
        snprintf(mat_name, sizeof(mat_name), "%s", name_item->valuestring);
    }
    else
    {
        GetFilenameStem(filepath, mat_name, sizeof(mat_name));
    }

    char mat_dir[1024] = {0};
    char can_save_path[1024];
    
    if (NormalizeAndCanonicalize(filepath, can_save_path, sizeof(can_save_path)))
        GetDirectoryPath(can_save_path, mat_dir, sizeof(mat_dir));
    else
        GetDirectoryPath(filepath, mat_dir, sizeof(mat_dir));


    // Shader validation
    cJSON* shader_item = cJSON_GetObjectItemCaseSensitive(root, "shader");
    cJSON* vert_item = NULL;
    cJSON* frag_item = NULL;

    if (shader_item)
    {
        if (!cJSON_IsNull(shader_item))
        {
            if (!cJSON_IsObject(shader_item))
            {
                cJSON_Delete(root);
                return NULL;
            }
            for (const cJSON* child = shader_item->child; child; child = child->next)
            {
                if (!child->string || (strcmp(child->string, "vertex") != 0 && strcmp(child->string, "fragment") != 0))
                {
                    cJSON_Delete(root);
                    return NULL;
                }
            }

            vert_item = cJSON_GetObjectItemCaseSensitive(shader_item, "vertex");
            frag_item = cJSON_GetObjectItemCaseSensitive(shader_item, "fragment");
            
            if (!vert_item || !frag_item || !cJSON_IsString(vert_item) || !cJSON_IsString(frag_item))
            {
                cJSON_Delete(root);
                return NULL;
            }
            
            if (vert_item->valuestring[0] == '\0' || frag_item->valuestring[0] == '\0' || IsAbsolutePath(vert_item->valuestring) || IsAbsolutePath(frag_item->valuestring))
            {
                cJSON_Delete(root);
                return NULL;
            }
        }
    }


    // Properties validation
    cJSON* props_item = cJSON_GetObjectItemCaseSensitive(root, "properties");
    MaterialProperties properties = {
        .albedo_tint = {1.0f, 1.0f, 1.0f, 1.0f},
        .metallic_factor = 0.0f,
        .roughness_factor = 0.5f
    };

    if (props_item)
    {
    	if (!cJSON_IsObject(props_item))
        {
            cJSON_Delete(root);
            return NULL;
        }

        for (const cJSON* child = props_item->child; child; child = child->next)
        {
            if (!child->string)
            {
                cJSON_Delete(root);
                return NULL;
            }

            if (strcmp(child->string, "albedo_tint") != 0 && strcmp(child->string, "metallic_factor") != 0 && strcmp(child->string, "roughness_factor") != 0)
            {
                cJSON_Delete(root);
                return NULL;
            }
        }

        cJSON* metal = cJSON_GetObjectItemCaseSensitive(props_item, "metallic_factor");
        if (metal)
        {
            if (!cJSON_IsNumber(metal) || isnan(metal->valuedouble) || isinf(metal->valuedouble) || metal->valuedouble < 0.0 || metal->valuedouble > 1.0)
            {
                cJSON_Delete(root);
                return NULL;
            }

            properties.metallic_factor = (float)metal->valuedouble;
        }

        cJSON* rough = cJSON_GetObjectItemCaseSensitive(props_item, "roughness_factor");
        if (rough)
        {
            if (!cJSON_IsNumber(rough) || isnan(rough->valuedouble) || isinf(rough->valuedouble) || rough->valuedouble < 0.0 || rough->valuedouble > 1.0)
            {
                cJSON_Delete(root);
                return NULL;
            }

            properties.roughness_factor = (float)rough->valuedouble;
        }

        cJSON* tint = cJSON_GetObjectItemCaseSensitive(props_item, "albedo_tint");
        if (tint)
        {
            if (!cJSON_IsArray(tint))
            {
                cJSON_Delete(root);
                return NULL;
            }

            int arr_size = cJSON_GetArraySize(tint);
            if (arr_size < 3 || arr_size > 4)
            {
                cJSON_Delete(root);
                return NULL;
            }

            float comp[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            for (int k = 0; k < arr_size; k++)
            {
                cJSON* el = cJSON_GetArrayItem(tint, k);
                if (!el || !cJSON_IsNumber(el) || isnan(el->valuedouble) || isinf(el->valuedouble) || el->valuedouble < 0.0 || el->valuedouble > 1.0)
                {
                    cJSON_Delete(root);
                    return NULL;
                }

                comp[k] = (float)el->valuedouble;
            }

            properties.albedo_tint = (Color){comp[0], comp[1], comp[2], comp[3]};
        }
    }


    // Textures validation
    cJSON* textures_item = cJSON_GetObjectItemCaseSensitive(root, "textures");
    if (textures_item)
    {
        if (!cJSON_IsObject(textures_item))
        {
            cJSON_Delete(root);
            return NULL;
        }

        for (const cJSON* child = textures_item->child; child; child = child->next)
        {
            if (!child->string)
            {
                cJSON_Delete(root);
                return NULL;
            }

            if (strcmp(child->string, "albedo") != 0 &&
                strcmp(child->string, "normal") != 0 &&
                strcmp(child->string, "metallic") != 0 &&
                strcmp(child->string, "roughness") != 0 &&
                strcmp(child->string, "ao") != 0)
            {
                cJSON_Delete(root);
                return NULL;
            }

            if (cJSON_IsNull(child))
                continue;

            if (cJSON_IsString(child))
            {
                if (child->valuestring[0] == '\0' || IsAbsolutePath(child->valuestring))
                {
                    cJSON_Delete(root);
                    return NULL;
                }

            #ifndef _WIN32
                char comb[1024];
                snprintf(comb, sizeof(comb), "%s%s", mat_dir, child->valuestring);
                char can[PATH_MAX];

                if (realpath(comb, can) == NULL)
                {
                    cJSON_Delete(root);
                    return NULL;
                }
            #endif
            }
            else if (cJSON_IsObject(child))
            {
                for (const cJSON* c = child->child; c; c = c->next)
                {
                    if (!c->string || strcmp(c->string, "color") != 0)
                    {
                        cJSON_Delete(root);
                        return NULL;
                    }
                }

                cJSON* col_arr = cJSON_GetObjectItemCaseSensitive(child, "color");
                if (!col_arr || !cJSON_IsArray(col_arr))
                {
                    cJSON_Delete(root);
                    return NULL;
                }

                int c_size = cJSON_GetArraySize(col_arr);
                if (c_size < 3 || c_size > 4)
                {
                    cJSON_Delete(root);
                    return NULL;
                }

                for (int k = 0; k < c_size; k++)
                {
                    cJSON* el = cJSON_GetArrayItem(col_arr, k);
                    if (!el || !cJSON_IsNumber(el) || isnan(el->valuedouble) || isinf(el->valuedouble) || el->valuedouble < 0.0 || el->valuedouble > 1.0)
                    {
                        cJSON_Delete(root);
                        return NULL;
                    }
                }
            }
            else
            {
                cJSON_Delete(root);
                return NULL;
            }
        }
    }

    // Now resolve dependencies, tracking newly allocated items for rollback
    Texture* newly_created_textures[5];
    int newly_created_texture_count = 0;
    Shader* newly_created_shader = NULL;

    Shader* target_shader = NULL;
    if (vert_item && frag_item)
    {
        char vert_comb[1024];
        char frag_comb[1024];
        snprintf(vert_comb, sizeof(vert_comb), "%s%s", mat_dir, vert_item->valuestring);
        snprintf(frag_comb, sizeof(frag_comb), "%s%s", mat_dir, frag_item->valuestring);

        char can_vert[1024];
        char can_frag[1024];
        
        if (!NormalizeAndCanonicalize(vert_comb, can_vert, sizeof(can_vert)) ||
            !NormalizeAndCanonicalize(frag_comb, can_frag, sizeof(can_frag)) ||
            !FileExists(can_vert) || !FileExists(can_frag))
        {
            cJSON_Delete(root);
            return NULL;
        }

        // Check if already in shader cache by paths
        for (uint32_t i = 0; i < am->shader_count; i++)
        {
            if (am->shader_cache[i].name[0] != '\0' && am->shader_vertex_paths[i] && am->shader_fragment_paths[i])
            {
                if (PathsEqual(am->shader_vertex_paths[i], can_vert) && PathsEqual(am->shader_fragment_paths[i], can_frag))
                {
                    target_shader = &am->shader_cache[i];
                    break;
                }
            }
        }

        if (!target_shader)
        {
            // Check cache capacity
            bool has_slot = false;
            for (uint32_t i = 0; i < am->shader_count; i++)
            {
                if (am->shader_cache[i].name[0] == '\0')
                {
                    has_slot = true;
                    break;
                }
            }

            if (!has_slot && am->shader_count >= MAX_CACHED_SHADERS)
            {
                cJSON_Delete(root);
                return NULL;
            }

            char shader_name[256];
            snprintf(shader_name, sizeof(shader_name), "Shader_%s_%s", GetBaseFilename(vert_item->valuestring), GetBaseFilename(frag_item->valuestring));
            target_shader = Asset_LoadShader(am, shader_name, can_vert, can_frag);

            if (!target_shader || target_shader->gpu_handle.id == 0)
            {
                if (target_shader)
                    Asset_UnloadShader(am, target_shader);

                cJSON_Delete(root);
                return NULL;
            }

            if (am->shader_vertex_paths[target_shader->id])
                free(am->shader_vertex_paths[target_shader->id]);

            if (am->shader_fragment_paths[target_shader->id])
                free(am->shader_fragment_paths[target_shader->id]);

            am->shader_vertex_paths[target_shader->id] = DuplicateString(can_vert);
            am->shader_fragment_paths[target_shader->id] = DuplicateString(can_frag);
            newly_created_shader = target_shader;
        }
    }


    // Helper macro for rollback on failure
    #define ROLLBACK_DEPENDENCIES() do { \
        for (int ri = 0; ri < newly_created_texture_count; ri++) { \
            Asset_UnloadTexture(am, newly_created_textures[ri]); \
        } \
        if (newly_created_shader) { \
            Asset_UnloadShader(am, newly_created_shader); \
        } \
    } while(0)


    // Resolve textures in order: albedo, normal, metallic, roughness, ao
    cJSON* alb_item = textures_item ? cJSON_GetObjectItemCaseSensitive(textures_item, "albedo") : NULL;
    cJSON* nrm_item = textures_item ? cJSON_GetObjectItemCaseSensitive(textures_item, "normal") : NULL;
    cJSON* met_item = textures_item ? cJSON_GetObjectItemCaseSensitive(textures_item, "metallic") : NULL;
    cJSON* rgh_item = textures_item ? cJSON_GetObjectItemCaseSensitive(textures_item, "roughness") : NULL;
    cJSON* ao_item  = textures_item ? cJSON_GetObjectItemCaseSensitive(textures_item, "ao") : NULL;


    bool is_new = false;

    Texture* albedo_tex = ResolveTextureSlot(am, alb_item, mat_dir, true, &is_new);
    if (!albedo_tex)
    {
        ROLLBACK_DEPENDENCIES();
        cJSON_Delete(root);
        return NULL;
    }

    if (is_new)
    	newly_created_textures[newly_created_texture_count++] = albedo_tex;

    Texture* normal_tex = ResolveTextureSlot(am, nrm_item, mat_dir, false, &is_new);
    if (nrm_item && !cJSON_IsNull(nrm_item) && !normal_tex)
    {
        ROLLBACK_DEPENDENCIES();
        cJSON_Delete(root);
        return NULL;
    }

    if (is_new)
    	newly_created_textures[newly_created_texture_count++] = normal_tex;

    Texture* metallic_tex = ResolveTextureSlot(am, met_item, mat_dir, false, &is_new);
    if (met_item && !cJSON_IsNull(met_item) && !metallic_tex)
    {
        ROLLBACK_DEPENDENCIES();
        cJSON_Delete(root);
        return NULL;
    }

    if (is_new)
    	newly_created_textures[newly_created_texture_count++] = metallic_tex;

    Texture* roughness_tex = ResolveTextureSlot(am, rgh_item, mat_dir, false, &is_new);
    if (rgh_item && !cJSON_IsNull(rgh_item) && !roughness_tex)
    {
        ROLLBACK_DEPENDENCIES();
        cJSON_Delete(root);
        return NULL;
    }

    if (is_new)
    	newly_created_textures[newly_created_texture_count++] = roughness_tex;

    Texture* ao_tex = ResolveTextureSlot(am, ao_item, mat_dir, false, &is_new);
    if (ao_item && !cJSON_IsNull(ao_item) && !ao_tex)
    {
        ROLLBACK_DEPENDENCIES();
        cJSON_Delete(root);
        return NULL;
    }

    if (is_new)
    	newly_created_textures[newly_created_texture_count++] = ao_tex;

    cJSON_Delete(root);

    // Create material
    Material* mat = Asset_CreateMaterial(am, mat_name, albedo_tex, target_shader);
    if (!mat)
    {
        ROLLBACK_DEPENDENCIES();
        return NULL;
    }

    mat->normal_map = normal_tex;
    mat->metallic_map = metallic_tex;
    mat->roughness_map = roughness_tex;
    mat->ao_map = ao_tex;
    mat->properties = properties;

    Asset_SyncMaterialGPU(am, mat);

    if (mat->gpu_handle.id == 0)
    {
        Asset_UnloadMaterial(am, mat);
        ROLLBACK_DEPENDENCIES();
        return NULL;
    }

    if (am->material_source_paths[mat->id])
        free(am->material_source_paths[mat->id]);

    am->material_source_paths[mat->id] = DuplicateString(canonical_mat_path);
    #undef ROLLBACK_DEPENDENCIES

    return mat;
}










static void MakePathRelative(const char* base_dir, const char* target_path, char* out_rel, size_t out_size)
{
    if (!target_path || !out_rel || out_size == 0)
    	return;

    if (!base_dir || base_dir[0] == '\0')
    {
        snprintf(out_rel, out_size, "%s", target_path);
        return;
    }

    char can_base[1024];
    char can_target[1024];

    if (!NormalizeAndCanonicalize(base_dir, can_base, sizeof(can_base)))
        snprintf(can_base, sizeof(can_base), "%s", base_dir);

    if (!NormalizeAndCanonicalize(target_path, can_target, sizeof(can_target)))
        snprintf(can_target, sizeof(can_target), "%s", target_path);

    size_t b_len = strlen(can_base);
    if (b_len > 0 && can_base[b_len - 1] != '/' && can_base[b_len - 1] != '\\')
    {
        if (b_len + 1 < sizeof(can_base))
        {
            can_base[b_len] = '/';
            can_base[b_len + 1] = '\0';
            b_len++;
        }
    }

#ifdef _WIN32
    if (can_base[1] == ':' && can_target[1] == ':' && tolower((unsigned char)can_base[0]) != tolower((unsigned char)can_target[0]))
    {
        snprintf(out_rel, out_size, "%s", can_target);
        return;
    }
#endif
    size_t common_slash = 0;
    size_t i = 0;
    while (can_base[i] && can_target[i])
    {
#ifdef _WIN32
        if (tolower((unsigned char)can_base[i]) != tolower((unsigned char)can_target[i]))
            break;
#else
        if (can_base[i] != can_target[i])
            break;
#endif
        if (can_base[i] == '/')
            common_slash = i + 1;

        i++;
    }

    if (common_slash == 0)
    {
        snprintf(out_rel, out_size, "%s", can_target);
        return;
    }

    out_rel[0] = '\0';
    size_t out_len = 0;

    for (size_t j = common_slash; j < b_len; j++)
    {
        if (can_base[j] == '/')
        {
            if (out_len + 3 < out_size)
            {
                strcat(out_rel, "../");
                out_len += 3;
            }
        }
    }

    const char* remainder = can_target + common_slash;
    
    if (out_len + strlen(remainder) < out_size)
        strcat(out_rel, remainder);
}










static cJSON* SerializeTextureSlot(AssetManager* am, Texture* tex, const char* mat_dir, bool is_albedo)
{
    if (!tex || (is_albedo && tex == am->default_texture))
        return cJSON_CreateNull();

    if (tex->id < MAX_CACHED_TEXTURES && am->texture_source_paths[tex->id])
    {
        char rel[1024];
        MakePathRelative(mat_dir, am->texture_source_paths[tex->id], rel, sizeof(rel));
        return cJSON_CreateString(rel);
    }

    // Check if solid color texture
    if (strncmp(tex->name, "SolidColor_", 11) == 0)
    {
        unsigned int rb, gb, bb, ab;

        if (sscanf(tex->name + 11, "%02x%02x%02x%02x", &rb, &gb, &bb, &ab) == 4)
        {
            cJSON* col_obj = cJSON_CreateObject();
            cJSON* arr = cJSON_CreateArray();
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(rb / 255.0));
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(gb / 255.0));
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(bb / 255.0));
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(ab / 255.0));
            cJSON_AddItemToObject(col_obj, "color", arr);
            return col_obj;
        }
    }

    return cJSON_CreateNull();
}










bool MaterialLoader_Save(AssetManager* am, Material* material, const char* filepath)
{
    if (!am || !material || !material->active || !filepath || filepath[0] == '\0')
        return false;

    cJSON* root = cJSON_CreateObject();
    if (!root)
    	return false;

    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddStringToObject(root, "name", material->name);

    char mat_dir[1024] = {0};
    char can_save_path[1024];
    
    if (NormalizeAndCanonicalize(filepath, can_save_path, sizeof(can_save_path)))
        GetDirectoryPath(can_save_path, mat_dir, sizeof(mat_dir));
    else
        GetDirectoryPath(filepath, mat_dir, sizeof(mat_dir));

    if (material->shader == NULL)
    {
        cJSON_AddNullToObject(root, "shader");
    }
    else
    {
        uint32_t s_id = material->shader->id;
        if (s_id < MAX_CACHED_SHADERS && am->shader_vertex_paths[s_id] && am->shader_fragment_paths[s_id])
        {
            cJSON* shader_obj = cJSON_CreateObject();
            char vert_rel[1024];
            char frag_rel[1024];
            MakePathRelative(mat_dir, am->shader_vertex_paths[s_id], vert_rel, sizeof(vert_rel));
            MakePathRelative(mat_dir, am->shader_fragment_paths[s_id], frag_rel, sizeof(frag_rel));
            cJSON_AddStringToObject(shader_obj, "vertex", vert_rel);
            cJSON_AddStringToObject(shader_obj, "fragment", frag_rel);
            cJSON_AddItemToObject(root, "shader", shader_obj);
        }
        else
        {
            cJSON_AddNullToObject(root, "shader");
        }
    }

    cJSON* props_obj = cJSON_CreateObject();
    cJSON* tint_arr = cJSON_CreateArray();
    cJSON_AddItemToArray(tint_arr, cJSON_CreateNumber(material->properties.albedo_tint.r));
    cJSON_AddItemToArray(tint_arr, cJSON_CreateNumber(material->properties.albedo_tint.g));
    cJSON_AddItemToArray(tint_arr, cJSON_CreateNumber(material->properties.albedo_tint.b));
    cJSON_AddItemToArray(tint_arr, cJSON_CreateNumber(material->properties.albedo_tint.a));
    cJSON_AddItemToObject(props_obj, "albedo_tint", tint_arr);
    cJSON_AddNumberToObject(props_obj, "metallic_factor", material->properties.metallic_factor);
    cJSON_AddNumberToObject(props_obj, "roughness_factor", material->properties.roughness_factor);
    cJSON_AddItemToObject(root, "properties", props_obj);

    cJSON* tex_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(tex_obj, "albedo", SerializeTextureSlot(am, material->albedo_texture, mat_dir, true));
    cJSON_AddItemToObject(tex_obj, "normal", SerializeTextureSlot(am, material->normal_map, mat_dir, false));
    cJSON_AddItemToObject(tex_obj, "metallic", SerializeTextureSlot(am, material->metallic_map, mat_dir, false));
    cJSON_AddItemToObject(tex_obj, "roughness", SerializeTextureSlot(am, material->roughness_map, mat_dir, false));
    cJSON_AddItemToObject(tex_obj, "ao", SerializeTextureSlot(am, material->ao_map, mat_dir, false));
    cJSON_AddItemToObject(root, "textures", tex_obj);

    char* json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json_str)
    	return false;

    FILE* f = fopen(filepath, "wb");
    if (!f)
    {
        free(json_str);
        return false;
    }

    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, f);
    fclose(f);
    free(json_str);

    return (written == len);
}