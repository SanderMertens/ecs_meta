#include <ecs_meta.h>
#include <stdarg.h>

static int indent = 0;

void print_indent(
    const char *fmt, 
    ...) 
{
    va_list args;
    printf("%*s", indent * 4, "");

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static
uint32_t print_value(
    ecs_world_t *world,
    void *base, 
    ecs_meta_cache_op_t *ops, 
    uint32_t op, 
    uint32_t op_count);

static
void print_primitive(
    ecs_world_t *world,
    void *base, 
    ecs_meta_cache_op_t *op) 
{
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
        printf("%llu", ecs_meta_get_uint(base, op));
        break;
    case EcsI8:
    case EcsI16:
    case EcsI32:
    case EcsI64:
        printf("%lld", ecs_meta_get_int(base, op));
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
        break;
    case EcsEntity: {
        ecs_entity_t e = ecs_meta_get_entity(base, op);
        EcsId *id = ecs_get_ptr(world, e, EcsId);
        if (!id) {
            printf("%lld", ecs_meta_get_entity(base, op));
        } else {
            printf("%s", *id);
        }
        break;
    }
    }
}

static
void print_enum(
    ecs_world_t *world,
    void *base, 
    ecs_meta_cache_op_t *op) 
{
    int32_t value = ecs_meta_get_enum(base, op);
    printf("%s", ecs_meta_enum_to_string(value, op));
}

static
void print_bitmask(
    ecs_world_t *world,
    void *base, 
    ecs_meta_cache_op_t *op) 
{
    ut_strbuf buf = UT_STRBUF_INIT;
    uint64_t value = ecs_meta_get_bitmask(base, op);
    ecs_meta_bitmask_to_string(value, op, &buf);
    char *str = ut_strbuf_get(&buf);
    printf("%s", str);
    free(str);
}

static
uint32_t print_struct(
    ecs_world_t *world,
    void *base, 
    ecs_meta_cache_op_t *ops, 
    uint32_t op, 
    uint32_t op_count)
{
    printf("{\n");
    indent ++;
    op = print_value(world, base, ops, op, op_count);
    indent --;
    print_indent("}\n");
    return op;
}

static
void print_vector(
    ecs_world_t *world,
    void *base,
    ecs_meta_cache_op_t *op)
{
    ecs_vector_t *vector = ecs_meta_get_vector(base, op);
    uint32_t count = ecs_vector_count(vector);
    void *element = ecs_vector_first(vector);
    
    ecs_vector_t *element_ops_vec = op->data.element_ops;
    ecs_meta_cache_op_t *element_ops = ecs_vector_first(element_ops_vec);
    uint32_t element_ops_count = ecs_vector_count(element_ops_vec);
    uint32_t size = element_ops[0].size;

    printf("[\n");
    indent ++;
    for (int i = 0; i < count; i ++) {
        print_value(world, element, element_ops, 0, element_ops_count);
        element = ECS_OFFSET(element, size);
    }
    indent --;
    print_indent("]");
}

static
uint32_t print_value(
    ecs_world_t *world,
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
                print_indent("%s: ", cur->name);
            } else {
                print_indent("");
            }

            switch(cur->kind) {
            case EcsOpPrimitive:
                print_primitive(world, base, cur);
                break;
            case EcsOpEnum:
                print_enum(world, base, cur);
                break;
            case EcsOpBitmask:
                print_bitmask(world, base, cur);
                break;
            case EcsOpPush: {
                uint32_t next_op = print_struct(world, base, ops, op + 1, op_count);
                if (i == cur->count - 1) {
                    op = next_op;
                } else {
                    base = ECS_OFFSET(base, size);
                }
            }
            case EcsOpPop:
                return op;
            case EcsOpArray:
                break;
            case EcsOpVector:
                print_vector(world, base, cur);
            case EcsOpMap:
            case EcsOpNothing:
                break;
            }

            printf("\n");
        }
    }

    return op;
}

void PrintValue(ecs_rows_t *rows) {
    ECS_COLUMN(rows, EcsId, id, 1);
    ECS_COLUMN_ENTITY(rows, component, 2);
    ECS_COLUMN_COMPONENT(rows, EcsMetaCache, 3);
    
    EcsMetaCache *p_cache = ecs_get_ptr(rows->world, component, EcsMetaCache);
    if (!p_cache) {
        printf("No cache for component %s\n", ecs_get_id(rows->world, component));
        return;
    }

    ecs_meta_cache_op_t *ops = ecs_vector_first(p_cache->ops);
    uint32_t op_count = ecs_vector_count(p_cache->ops);
    void *base = _ecs_column(rows, 0, 2);

    /* Loop entities */
    for (uint32_t e = 0; e < rows->count; e ++) {
        printf("%s: ", id[e]);
        print_value(rows->world, base, ops, 0, op_count);
        base = ECS_OFFSET(base, ops[0].size);
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    ecs_world_t *world = ecs_init_w_args(argc, argv);

    ECS_IMPORT(world, FlecsComponentsMeta, 0);

    ECS_SYSTEM(world, PrintValue, EcsManual, EcsId, EcsMetaStruct, .EcsMetaCache);

    ecs_run(world, PrintValue, 0, NULL);

    return ecs_fini(world);
}
