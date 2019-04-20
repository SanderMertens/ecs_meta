#include <include/ecs_meta.h>

static
uint32_t print_value(
    void *base, 
    ecs_meta_cache_op_t *ops, 
    uint32_t op, 
    uint32_t op_count);

static
void print_primitive(void *base, ecs_meta_cache_op_t *op) {
    switch(op->data.primitive_kind) {
    case EcsBool:
        if (ecs_meta_get_bool(base, op)) {
            printf("true");
        } else {
            printf("false");
        }
        break;
    case EcsChar:
        printf("%c", ecs_meta_get_char(base, op));
        break;
    case EcsByte:
        printf("%x", ecs_meta_get_char(base, op));
        break;
    case EcsU8:
    case EcsU16:
    case EcsU32:
    case EcsU64:
        printf("%lu", ecs_meta_get_uint(base, op));
        break;
    case EcsI8:
    case EcsI16:
    case EcsI32:
    case EcsI64:
        printf("%ld", ecs_meta_get_int(base, op));
        break;
    case EcsF32:
    case EcsF64:
        printf("%f", ecs_meta_get_float(base, op));
        break;
    case EcsWord:
        printf("%lx", ecs_meta_get_word(base, op));
        break;
    case EcsString:
        printf("\"%s\"", ecs_meta_get_string(base, op));
    case EcsEntity:
        printf("%ld", ecs_meta_get_entity(base, op));
        break;
    }
}

static
void print_enum(void *base, ecs_meta_cache_op_t *op) {
    int32_t value = ecs_meta_get_enum(base, op);
    printf("%s", ecs_meta_enum_to_string(value, op));
}

static
void print_bitmask(void *base, ecs_meta_cache_op_t *op) {
    ut_strbuf buf = UT_STRBUF_INIT;
    uint64_t value = ecs_meta_get_bitmask(base, op);
    ecs_meta_bitmask_to_string(value, op, &buf);
    char *str = ut_strbuf_get(&buf);
    printf("%s", str);
    free(str);
}

static
uint32_t print_struct(
    void *base, 
    ecs_meta_cache_op_t *ops, 
    uint32_t op, 
    uint32_t op_count)
{
    printf("{\n");
    op = print_value(base, ops, op, op_count);
    printf("}\n");
    return op;
}

static
uint32_t print_value(
    void *base, 
    ecs_meta_cache_op_t *ops, 
    uint32_t op, 
    uint32_t op_count) 
{
    /* Loop operations */
    for (; op < op_count; op ++) {
        ecs_meta_cache_op_t *cur = &ops[op];
        int32_t size = cur->size;

        /* Loop count (>1 for arrays) */
        for (uint32_t i = 0; i < cur->count; i ++) {
            if (cur->name) {
                printf("%s: ", cur->name);
            }

            switch(cur->kind) {
            case EcsOpPrimitive:
                print_primitive(base, cur);
                break;
            case EcsOpEnum:
                print_enum(base, cur);
                break;
            case EcsOpBitmask:
                print_bitmask(base, cur);
                break;
            case EcsOpPush:
                op = print_struct(base, ops, op + 1, op_count);
            case EcsOpPop:
                return op;
            case EcsOpArray:
            case EcsOpVector:
            case EcsOpMap:
            case EcsOpNothing:
                break;
            }

            printf("\n");
        }
    }
}

void PrintValue(ecs_rows_t *rows) {
    ECS_COLUMN_ENTITY(rows, EcsPosition2D, 1);
    ECS_COLUMN_COMPONENT(rows, EcsMetaCache, 2);
    
    EcsMetaCache *p_cache = ecs_get_ptr(rows->world, EcsPosition2D, EcsMetaCache);
    if (!p_cache) {
        printf("No cache for component %s\n", ecs_get_id(rows->world, EcsPosition2D));
        return;
    }

    ecs_meta_cache_op_t *ops = ecs_vector_first(p_cache->ops);
    uint32_t op_count = ecs_vector_count(p_cache->ops);
    void *base = _ecs_column(rows, 1, false);

    /* Loop entities */
    for (uint32_t e = 0; e < rows->count; e ++) {
        print_value(base, ops, 0, op_count);
        base = ECS_OFFSET(base, ops[0].size);
    }
}

int main(int argc, char *argv[]) {
    /* Create the world, pass arguments for overriding the number of threads,fps
     * or for starting the admin dashboard (see flecs.h for details). */
    ecs_world_t *world = ecs_init_w_args(argc, argv);

    ECS_IMPORT(world, EcsComponentsMeta, 0);
    ECS_IMPORT(world, EcsComponentsTransform, ECS_REFLECTION);

    // TODO - load flecs.components.transform at runtime */
    //ecs_import_from_library(world, "flecs.components.transform", "EcsComponentsTransform", 0);

    ECS_TYPE(world, Type, EcsPosition2D);
    ECS_SYSTEM(world, PrintValue, EcsManual, EcsPosition2D, ID.EcsMetaCache);

    for (int i = 0; i < 10; i ++) {
        ecs_set(world, 0, EcsPosition2D, {i, i * 2});
    }

    ecs_run(world, PrintValue, 0, NULL);

    /* Cleanup */
    return ecs_fini(world);
}
