#pragma once
#include <fstream>
#include <string>

// Minimal DAG that matches the planned stages.
inline void emit_dag_json(const std::string& out_path) {
    std::ofstream f(out_path, std::ios::binary);
    if (!f) return;

    f << R"({
  "version":"1",
  "nodes":[
    {"id":"n1","label":"read_chunks","type":"io","duration_ms":0.0,"rows_in":null,"rows_out":null,"bytes_in":0,"bytes_out":0},
    {"id":"n2","label":"tokenize_csv","type":"parse","duration_ms":0.0,"rows_in":null,"rows_out":0,"bytes_in":0,"bytes_out":0},
    {"id":"n3","label":"type_infer","type":"analyze","duration_ms":0.0,"rows_in":0,"rows_out":0,"bytes_in":null,"bytes_out":null},
    {"id":"n4","label":"profile_columns","type":"profile","duration_ms":0.0,"rows_in":0,"rows_out":0,"bytes_in":null,"bytes_out":null},
    {"id":"n5","label":"emit_report","type":"render","duration_ms":0.0,"rows_in":null,"rows_out":null,"bytes_in":null,"bytes_out":null}
  ],
  "edges":[
    {"from":"n1","to":"n2"},
    {"from":"n2","to":"n3"},
    {"from":"n3","to":"n4"},
    {"from":"n4","to":"n5"}
  ]
})";
}
