struct DrawIndexedInstancedCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    uint first_vertex;
    uint first_instance;
};

struct DrawInstancedCommand {
    uint vertex_cont;
    uint instance_count;
    uint first_vertex;
    uint first_instance;
};

struct DispatchCommand {
    uint work_group_count_x;
    uint work_group_count_y;
    uint work_group_count_z;
};