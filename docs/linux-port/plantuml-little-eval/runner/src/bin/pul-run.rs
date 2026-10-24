// pul-run IN.puml OUT.svg : one conversion per process, so a panic, stack
// overflow or hang costs only that file. PUL_BACKEND=wasm routes Graphviz
// through the crate's own test backend (Node + the wasm Graphviz its Java
// references use); otherwise the native graphviz-anywhere library, as a
// bundled Tinta would ship it. Prints "ok <ms> <bytes>" or "error <ms> <msg>".
#[path = "../../../supramark/crates/plantuml-little/tests/support/wasm_backend.rs"]
#[allow(dead_code)]
mod wasm_backend;

use std::time::Instant;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if std::env::var("PUL_BACKEND").as_deref() == Ok("wasm") {
        plantuml_little::layout::graphviz::set_custom_dot_renderer(|dot| wasm_backend::render_dot_to_svg(dot));
    }
    let source = std::fs::read_to_string(&args[1]).expect("read input");
    let start = Instant::now();
    let result = plantuml_little::convert(&source);
    let ms = start.elapsed().as_secs_f64() * 1000.0;
    match result {
        Ok(svg) => {
            std::fs::write(&args[2], &svg).expect("write svg");
            println!("ok\t{ms:.1}\t{}", svg.len());
        }
        Err(e) => println!("error\t{ms:.1}\t{}", e.to_string().replace(['\n', '\t'], " ")),
    }
}
