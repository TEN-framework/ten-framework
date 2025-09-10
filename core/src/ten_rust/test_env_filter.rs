use tracing_subscriber::EnvFilter;

fn main() {
    // Test various filter directive formats
    let directives = vec![
        "off,auth=info,database=debug",
        "auth=info,database=debug", 
        "info,auth=info,database=debug",
    ];
    
    for directive in directives {
        println!("Testing directive: {}", directive);
        match EnvFilter::try_new(directive) {
            Ok(_) => println!("  ✓ Valid"),
            Err(e) => println!("  ✗ Error: {}", e),
        }
    }
}
