(function () {
  // ---------- tiny helpers ----------
  function $(sel){ return document.querySelector(sel); }
  function setText(el, s){ if (el) el.textContent = s; }
  function getRaw(id){ var n=document.getElementById(id); return n ? (n.textContent || "") : ""; }
  function safeParse(txt){ try { return JSON.parse(txt); } catch(e){ return null; } }
  function isArr(v){ return Object.prototype.toString.call(v) === "[object Array]"; }
  function fmtMB(b){ return (b/1048576).toFixed(2) + " MB"; }

  // ---------- embed helper ----------
  function tryEmbed(sel, spec){
    var el = $(sel);
    if (!el) return;

    function _embed() {
      if (!window.vegaEmbed) {
        setText(el, "Charts unavailable (Vega not loaded).");
        return;
      }
      // width:"container" can resolve to 0 if layout not ready; fallback to fixed width
      var w = el.clientWidth || el.getBoundingClientRect().width || 0;
      var specWithWidth = spec;
      if (!w) {
        try { specWithWidth = JSON.parse(JSON.stringify(spec)); } catch(_) { specWithWidth = spec; }
        specWithWidth.width = 640;
      }
      window.vegaEmbed(el, specWithWidth, { actions:false, renderer:"canvas" })
        .catch(function(err){
          setText(el, "Failed to render chart.");
          if (window.console) console.error("[csvqr] Vega error on", sel, err);
        });
    }

    if ((el.clientWidth || 0) === 0){
      requestAnimationFrame(_embed);
    } else {
      _embed();
    }
  }

  // ---------- core (populate KPIs/tables/dag) ----------
  function renderCore(){
    var run      = safeParse(getRaw("run_json"))      || {};
    var profile  = safeParse(getRaw("profile_json"))  || {};
    var dag      = safeParse(getRaw("dag_json"))      || {};

    var ds      = (profile && profile.dataset) ? profile.dataset : {};
    var stages  = isArr(run.stages) ? run.stages : [];
    var samples = isArr(run.samples) ? run.samples : [];
    var cols    = isArr(profile.columns) ? profile.columns : [];

    // Meta + KPIs
    var src = ds.source_path || "(unknown)";
    var rows = (typeof run.rows === "number" && isFinite(run.rows)) ? run.rows : 0;
    var colsCount = (typeof ds.columns === "number" && isFinite(ds.columns)) ? ds.columns : (cols.length || 0);
    setText($("#meta"), "Source: " + src + " • Rows: " + rows + " • Cols: " + colsCount);

    var v;
    v = $("#kpi-rows");         if (v) setText(v, (rows.toLocaleString ? rows.toLocaleString() : String(rows)));
    v = $("#kpi-cols");         if (v) setText(v, colsCount ? (colsCount + " columns") : "—");
    v = $("#kpi-errors");       if (v) setText(v, (run.errors != null ? run.errors : 0));
    v = $("#kpi-wall");         if (v) setText(v, ((run.wall_time_ms || 0).toFixed ? run.wall_time_ms.toFixed(3) : String(run.wall_time_ms || 0)));
    v = $("#kpi-startend");     if (v) setText(v, (run.started_at && run.ended_at) ? (run.started_at + " → " + run.ended_at) : "—");
    v = $("#kpi-mbps");         if (v) setText(v, ((run.throughput_input_mb_s || 0).toFixed ? run.throughput_input_mb_s.toFixed(3) : String(run.throughput_input_mb_s || 0)));
    var totalBytes = (typeof run.input_bytes === "number" && isFinite(run.input_bytes)) ? run.input_bytes : 0;
    v = $("#kpi-bytes");        if (v) setText(v, totalBytes ? (fmtMB(totalBytes) + " (" + (totalBytes.toLocaleString ? totalBytes.toLocaleString() : String(totalBytes)) + " bytes)") : "—");
    v = $("#kpi-rss");          if (v) setText(v, ((run.rss_peak_mb || 0).toFixed ? run.rss_peak_mb.toFixed(2) : String(run.rss_peak_mb || 0)));
    v = $("#kpi-header");       if (v) setText(v, ds.header_present ? "Yes" : "No");

    // Stages table
    var tbody = $("#stages-table tbody");
    if (tbody) {
      if (!stages.length) {
        tbody.innerHTML = '<tr><td colspan="4" class="small">No stages recorded.</td></tr>';
      } else {
        var rowsHtml = [];
        for (var i=0;i<stages.length;i++){
          var s = stages[i];
          var p50 = (s.p50_ms != null && typeof s.p50_ms === "number") ? s.p50_ms.toFixed(3) : "—";
          var p95 = (s.p95_ms != null && typeof s.p95_ms === "number") ? s.p95_ms.toFixed(3) : "—";
          rowsHtml.push(
            "<tr><td>" + (s.name || "(unnamed)") + "</td><td>" +
            (s.calls != null ? s.calls : 0) + "</td><td>" + p50 + "</td><td>" + p95 + "</td></tr>"
          );
        }
        tbody.innerHTML = rowsHtml.join("");
      }
    }

    // DAG mini summary
    var nodes = isArr(dag.nodes) ? dag.nodes : [];
    var edges = isArr(dag.edges) ? dag.edges : [];
    setText($("#dag-overview"), (nodes.length + " nodes • " + edges.length + " edges"));
    var dge = $("#dag-edges");
    if (dge) {
      if (!edges.length) {
        dge.innerHTML = "<li>No edges</li>";
      } else {
        var ehtml = [], lim = Math.min(20, edges.length);
        for (var j=0;j<lim;j++) { var e = edges[j]; ehtml.push("<li>" + e.from + " → " + e.to + "</li>"); }
        dge.innerHTML = ehtml.join("");
      }
    }

    return { stages: stages, samples: samples, cols: cols };
  }

  // ---------- charts (compute in JS; no VL transforms) ----------
  function renderCharts(state){
    var stages  = state.stages  || [];
    var samples = state.samples || [];
    var cols    = state.cols    || [];

    if (!window.vegaEmbed){
      ["#chart-throughput","#chart-stage","#chart-null","#chart-mem","#chart-cpu"].forEach(function(s){
        setText($(s), "Charts unavailable (Vega not loaded).");
      });
      return;
    }

    // Throughput: {ts, mb}
    (function(){
      var arr = [];
      for (var i=0;i<samples.length;i++){
        var s = samples[i];
        if (typeof s.ts_ms === "number") arr.push({ ts: +s.ts_ms, mb: (+s.bytes_in || 0)/1048576 });
      }
      if (arr.length) {
        tryEmbed("#chart-throughput", {
          $schema:"https://vega.github.io/schema/vega-lite/v5.json",
          width:"container", height:160,
          data:{ values: arr },
          mark:{ type:"line", interpolate:"monotone" },
          encoding:{
            x:{ field:"ts", type:"quantitative", title:"time (ms)" },
            y:{ field:"mb", type:"quantitative", title:"MB in" },
            tooltip:[ {field:"ts", title:"ms"}, {field:"mb", title:"MB", format:".2f"} ]
          }
        });
      } else {
        setText($("#chart-throughput"), "No throughput samples.");
      }
    })();

    // Stages p95: {name, v, calls}
    (function(){
      if (!stages.length){ setText($("#chart-stage"), "No stage metrics."); return; }
      var arr = [];
      for (var i=0;i<stages.length;i++){
        var st = stages[i];
        arr.push({ name: st.name || "(unnamed)", v: +(st.p95_ms || 0), calls: +(st.calls || 0) });
      }
      tryEmbed("#chart-stage", {
        $schema:"https://vega.github.io/schema/vega-lite/v5.json",
        width:"container", height:180,
        data:{ values: arr },
        mark:"bar",
        encoding:{
          x:{ field:"name", type:"nominal", title:null },
          y:{ field:"v", type:"quantitative", title:"p95 (ms)" },
          tooltip:[ {field:"name", title:"stage"}, {field:"v", title:"p95 (ms)", format:".3f"}, {field:"calls", title:"calls"} ]
        }
      });
    })();

    // Null ratio: {name, ratio}
    (function(){
      if (!cols.length){ setText($("#chart-null"), "No column profile data."); return; }
      var arr = [];
      for (var i=0;i<cols.length;i++){
        var c = cols[i], nn=+(c.non_null_count||0), nul=+(c.null_count||0), tot=nn+nul;
        arr.push({ name: c.name || "(unknown)", ratio: (tot>0 ? (nul/tot) : 0), null_count: nul, non_null_count: nn });
      }
      tryEmbed("#chart-null", {
        $schema:"https://vega.github.io/schema/vega-lite/v5.json",
        width:"container", height:180,
        data:{ values: arr },
        mark:"bar",
        encoding:{
          x:{ field:"name", type:"nominal", title:null },
          y:{ field:"ratio", type:"quantitative", title:"null ratio", axis:{ format:".0%"} },
          tooltip:[
            {field:"name", title:"column"},
            {field:"ratio", title:"null ratio", format:".1%"},
            {field:"null_count", title:"nulls"},
            {field:"non_null_count", title:"non-nulls"}
          ]
        }
      });
    })();

    // Memory: {ts, rss}
    (function(){
      var arr = [];
      for (var i=0;i<samples.length;i++){
        var s = samples[i];
        if (typeof s.rss_mb === "number") arr.push({ ts:+s.ts_ms, rss:+s.rss_mb });
      }
      if (arr.length){
        tryEmbed("#chart-mem", {
          $schema:"https://vega.github.io/schema/vega-lite/v5.json",
          width:"container", height:160,
          data:{ values: arr }, mark:"line",
          encoding:{
            x:{ field:"ts", type:"quantitative", title:"time (ms)" },
            y:{ field:"rss", type:"quantitative", title:"RSS (MB)" },
            tooltip:[ {field:"ts", title:"ms"}, {field:"rss", title:"RSS (MB)", format:".2f"} ]
          }
        });
      } else {
        setText($("#chart-mem"), "No memory samples.");
      }
    })();

    // CPU: {ts, cpu}
    (function(){
      var arr = [];
      for (var i=0;i<samples.length;i++){
        var s = samples[i];
        if (typeof s.cpu_pct === "number") arr.push({ ts:+s.ts_ms, cpu:+s.cpu_pct });
      }
      if (arr.length){
        tryEmbed("#chart-cpu", {
          $schema:"https://vega.github.io/schema/vega-lite/v5.json",
          width:"container", height:160,
          data:{ values: arr }, mark:"line",
          encoding:{
            x:{ field:"ts", type:"quantitative", title:"time (ms)" },
            y:{ field:"cpu", type:"quantitative", title:"CPU (%)" },
            tooltip:[ {field:"ts", title:"ms"}, {field:"cpu", title:"CPU (%)", format:".1f"} ]
          }
        });
      } else {
        setText($("#chart-cpu"), "No CPU samples.");
      }
    })();
  }

  // ---------- boot + lightweight retry ----------
  function boot(){
    var state = renderCore();
    renderCharts(state);

    // If vega libs were late for any reason, retry briefly.
    if (!window.vegaEmbed){
      var tries = 0, id = setInterval(function(){
        tries++;
        if (window.vegaEmbed){ clearInterval(id); renderCharts(state); }
        if (tries > 40) { clearInterval(id); }
      }, 80);
    }

    // Re-render charts on resize (debounced)
    var t=null;
    window.addEventListener("resize", function(){
      clearTimeout(t);
      t = setTimeout(function(){ renderCharts(state); }, 120);
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", boot);
  } else {
    boot();
  }
})();
