// svg-cmp : reads "ACTUAL\tREFERENCE" lines on stdin and prints one verdict
// per line, using plantuml-little's own reference-test comparator unchanged:
// byte (identical), canonical (equal after its normalisation chain), fuzzy
// (its ±2.51 pt numeric tolerance), or differs, with the first difference.
#[path = "../../../supramark/crates/plantuml-little/tests/support/svg_compare.rs"]
mod svg_compare;

use std::io::BufRead;

fn main() {
    std::panic::set_hook(Box::new(|_| {}));
    for line in std::io::stdin().lock().lines() {
        let line = line.expect("stdin");
        let (actual_path, reference_path) = line.split_once('\t').expect("two paths");
        let actual = std::fs::read_to_string(actual_path).unwrap_or_default();
        let reference = std::fs::read_to_string(reference_path).unwrap_or_default();
        let verdict = if actual == reference {
            "byte".to_string()
        } else {
            let a = actual.clone();
            let r = reference.clone();
            match std::panic::catch_unwind(move || svg_compare::assert_exact_match(&a, &r, "corpus")) {
                Ok(()) => {
                    let canon = |s: &str| {
                        svg_compare::normalize_error_page_noise(&svg_compare::normalize_arrow_polygons(
                            &svg_compare::normalize_inline_pngs(&svg_compare::normalize_entity_link_ids(
                                &svg_compare::normalize_filter_ids(&svg_compare::strip_nonvisual_data_attrs(
                                    &svg_compare::strip_plantuml_src_pi(s),
                                )),
                            )),
                        ))
                    };
                    if canon(&actual) == canon(&reference) { "canonical" } else { "fuzzy" }.to_string()
                }
                Err(e) => {
                    let msg = e
                        .downcast_ref::<String>()
                        .cloned()
                        .or_else(|| e.downcast_ref::<&str>().map(|s| s.to_string()))
                        .unwrap_or_default();
                    format!("differs\t{}", msg.replace(['\n', '\t'], " ").chars().take(400).collect::<String>())
                }
            }
        };
        println!("{actual_path}\t{verdict}");
    }
}
