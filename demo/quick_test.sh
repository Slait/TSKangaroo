#!/bin/bash

# Quick Test Script for RCKangaroo Distributed System
# This script demonstrates setting up and testing the system

set -e

echo "================================================="
echo "RCKangaroo Distributed System - Quick Test"
echo "================================================="

# Configuration
SERVER_PORT=8080
SERVER_URL="http://localhost:$SERVER_PORT"
TEST_RANGE_START="0x200000000000000000"
TEST_RANGE_END="0x400000000000000000"
TEST_PUBKEY="0290e6900a58d33393bc1097b5aed31f2e4e7cbd3e5466af958665bc0121248483"
TEST_DP_BITS=14
TEST_RANGE_SIZE="0x10000000000000"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check dependencies
check_dependencies() {
    log_info "Checking dependencies..."
    
    # Check Python
    if ! command -v python3 &> /dev/null; then
        log_error "Python 3 not found"
        exit 1
    fi
    
    # Check Flask
    if ! python3 -c "import flask" 2>/dev/null; then
        log_warning "Flask not found, installing..."
        pip3 install flask
    fi
    
    # Check if server directory exists
    if [ ! -d "../server" ]; then
        log_error "Server directory not found. Run this script from demo/ directory"
        exit 1
    fi
    
    # Check if client build files exist
    if [ ! -d "../client" ]; then
        log_error "Client directory not found"
        exit 1
    fi
    
    log_info "Dependencies check completed"
}

# Start server in background
start_server() {
    log_info "Starting server on port $SERVER_PORT..."
    
    cd ../server
    python3 kangaroo_server.py --port $SERVER_PORT --host 127.0.0.1 &
    SERVER_PID=$!
    cd ../demo
    
    # Wait for server to start
    log_info "Waiting for server to start..."
    for i in {1..30}; do
        if curl -s "$SERVER_URL/api/status" >/dev/null 2>&1; then
            log_info "Server started successfully (PID: $SERVER_PID)"
            return 0
        fi
        sleep 1
    done
    
    log_error "Server failed to start"
    exit 1
}

# Configure search on server
configure_server() {
    log_info "Configuring server search parameters..."
    
    # Try configuring via Python client if available, otherwise via curl
    if [ -f "../client/RCKangarooClient" ]; then
        log_info "Using compiled client for configuration..."
        cd ../client
        ./RCKangarooClient -server "$SERVER_URL" -configure \
            "$TEST_RANGE_START" "$TEST_RANGE_END" "$TEST_PUBKEY" \
            "$TEST_DP_BITS" "$TEST_RANGE_SIZE"
        cd ../demo
    else
        log_info "Using curl for configuration..."
        curl -X POST "$SERVER_URL/api/configure" \
            -H "Content-Type: application/json" \
            -d "{
                \"start_range\": \"$TEST_RANGE_START\",
                \"end_range\": \"$TEST_RANGE_END\",
                \"pubkey\": \"$TEST_PUBKEY\",
                \"dp_bits\": $TEST_DP_BITS,
                \"range_size\": \"$TEST_RANGE_SIZE\"
            }"
        echo
    fi
    
    log_info "Server configuration completed"
}

# Check server status
check_status() {
    log_info "Checking server status..."
    
    STATUS=$(curl -s "$SERVER_URL/api/status")
    echo "Server Status:"
    echo "$STATUS" | python3 -m json.tool 2>/dev/null || echo "$STATUS"
    echo
}

# Simulate client work submission
simulate_client() {
    log_info "Simulating client work..."
    
    # Get work assignment
    WORK=$(curl -s -X POST "$SERVER_URL/api/get_work" \
        -H "Content-Type: application/json" \
        -d '{"client_id": "test_client_001"}')
    
    echo "Work Assignment:"
    echo "$WORK" | python3 -m json.tool 2>/dev/null || echo "$WORK"
    echo
    
    # Simulate finding some distinguished points
    log_info "Simulating distinguished points submission..."
    
    POINTS='[
        {
            "x_coord": "a1b2c3d4e5f6",
            "distance": "123456789abcdef0123456789abcdef0123456",
            "kang_type": 1
        },
        {
            "x_coord": "f6e5d4c3b2a1",
            "distance": "fedcba9876543210fedcba9876543210fedcba",
            "kang_type": 2
        }
    ]'
    
    RESULT=$(curl -s -X POST "$SERVER_URL/api/submit_points" \
        -H "Content-Type: application/json" \
        -d "{\"client_id\": \"test_client_001\", \"points\": $POINTS}")
    
    echo "Submission Result:"
    echo "$RESULT" | python3 -m json.tool 2>/dev/null || echo "$RESULT"
    echo
}

# Test client compilation if source available
test_client_build() {
    if [ -d "../client" ] && [ -f "../client/Makefile" ]; then
        log_info "Testing client build..."
        cd ../client
        
        # Check dependencies
        if make deps 2>/dev/null; then
            log_info "Client dependencies satisfied"
            
            # Try to build
            if make 2>/dev/null; then
                log_info "Client built successfully"
                CLIENT_BUILT=true
            else
                log_warning "Client build failed - continuing without compiled client"
                CLIENT_BUILT=false
            fi
        else
            log_warning "Client dependencies not satisfied - skipping build"
            CLIENT_BUILT=false
        fi
        
        cd ../demo
    else
        log_warning "Client source not available - skipping build test"
        CLIENT_BUILT=false
    fi
}

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    
    if [ ! -z "$SERVER_PID" ]; then
        log_info "Stopping server (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
    
    # Clean up any test database files
    rm -f ../server/kangaroo_server.db
    
    log_info "Cleanup completed"
}

# Set up signal handlers
trap cleanup EXIT INT TERM

# Main test sequence
main() {
    echo "Starting quick test sequence..."
    echo
    
    check_dependencies
    echo
    
    test_client_build
    echo
    
    start_server
    echo
    
    # Give server a moment to fully initialize
    sleep 2
    
    configure_server
    echo
    
    check_status
    echo
    
    simulate_client
    echo
    
    check_status
    echo
    
    log_info "Test sequence completed successfully!"
    echo
    
    log_info "You can now:"
    echo "  1. View server status at: $SERVER_URL"
    echo "  2. Check API at: $SERVER_URL/api/status"
    if [ "$CLIENT_BUILT" = true ]; then
        echo "  3. Run real client: cd ../client && ./RCKangarooClient -server $SERVER_URL"
    else
        echo "  3. Build client manually: cd ../client && make"
    fi
    echo
    
    read -p "Press Enter to stop the server and exit..."
}

# Check if running as source
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi