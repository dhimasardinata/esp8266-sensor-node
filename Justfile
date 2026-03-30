# Justfile for Node Medini

default:
  @just --list

# Build the firmware
build:
  pio run

# Upload to device (USB)
upload:
  pio run -t upload

# Clean build artifacts
clean:
  pio run -t clean

# Run Static Analysis (Thorough)
check:
  pio check --fail-on-defect=medium --skip-packages

# Run Native Peak Simulation
test-sim:
  pio test -e native -f test_simulation -v

# Run ApiClient Logic Tests
test-logic:
  pio test -e native -f test_apiclient_flow -v

# Run All Tests
test-all:
  pio test -e native -v

# Deploy Docs (Manual)
deploy-docs:
  @echo "Deploying to Netlify (requires NETLIFY_AUTH_TOKEN env var)..."
  npx netlify-cli deploy --prod --dir=docs
