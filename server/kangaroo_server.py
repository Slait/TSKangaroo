#!/usr/bin/env python3
"""
RCKangaroo Server - Distributed ECDLP solving server
Manages centralized Distinguished Points database and work distribution
"""

import json
import time
import sqlite3
import hashlib
import threading
from datetime import datetime
from flask import Flask, request, jsonify
from dataclasses import dataclass
from typing import List, Optional, Dict, Tuple
import argparse
import logging
from collision_solver import CollisionSolver

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

@dataclass
class WorkRange:
    """Represents a work range assigned to a client"""
    start_hex: str
    end_hex: str
    bit_range: int
    dp_bits: int
    assigned_to: str
    assigned_at: float
    status: str  # 'assigned', 'completed', 'failed'

@dataclass
class DistinguishedPoint:
    """Represents a Distinguished Point found by a client"""
    x_coord: str  # hex string of x coordinate (12 bytes)
    distance: str  # hex string of distance (22 bytes) 
    kang_type: int  # 0=TAME, 1=WILD1, 2=WILD2
    client_id: str
    found_at: float

class KangarooServer:
    def __init__(self, db_path: str = "kangaroo_server.db"):
        self.db_path = db_path
        self.lock = threading.RLock()
        self.work_ranges: Dict[str, WorkRange] = {}
        self.current_range_start = None
        self.current_range_end = None
        self.range_size = None
        self.dp_bits = None
        self.pubkey = None
        self.solved = False
        self.solution = None
        self.collision_solver = None
        
        # Initialize database
        self._init_database()
        
        # Load existing state
        self._load_state()
        
    def _init_database(self):
        """Initialize SQLite database for storing DPs and work ranges"""
        with sqlite3.connect(self.db_path) as conn:
            conn.execute('''
                CREATE TABLE IF NOT EXISTS distinguished_points (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    x_coord TEXT NOT NULL,
                    distance TEXT NOT NULL,
                    kang_type INTEGER NOT NULL,
                    client_id TEXT NOT NULL,
                    found_at REAL NOT NULL,
                    UNIQUE(x_coord)
                )
            ''')
            
            conn.execute('''
                CREATE TABLE IF NOT EXISTS work_ranges (
                    range_id TEXT PRIMARY KEY,
                    start_hex TEXT NOT NULL,
                    end_hex TEXT NOT NULL,
                    bit_range INTEGER NOT NULL,
                    dp_bits INTEGER NOT NULL,
                    assigned_to TEXT,
                    assigned_at REAL,
                    status TEXT DEFAULT 'pending'
                )
            ''')
            
            conn.execute('''
                CREATE TABLE IF NOT EXISTS server_state (
                    key TEXT PRIMARY KEY,
                    value TEXT NOT NULL
                )
            ''')
            
            conn.execute('CREATE INDEX IF NOT EXISTS idx_dp_x_coord ON distinguished_points(x_coord)')
            conn.execute('CREATE INDEX IF NOT EXISTS idx_work_status ON work_ranges(status)')
            
    def _load_state(self):
        """Load server state from database"""
        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.execute('SELECT key, value FROM server_state')
            state = dict(cursor.fetchall())
            
            self.current_range_start = state.get('current_range_start')
            self.current_range_end = state.get('current_range_end')
            self.range_size = int(state.get('range_size', '0')) if state.get('range_size') else None
            self.dp_bits = int(state.get('dp_bits', '0')) if state.get('dp_bits') else None
            self.pubkey = state.get('pubkey')
            self.solved = state.get('solved', 'false').lower() == 'true'
            self.solution = state.get('solution')
            
    def _save_state(self):
        """Save server state to database"""
        with sqlite3.connect(self.db_path) as conn:
            state_data = [
                ('current_range_start', self.current_range_start or ''),
                ('current_range_end', self.current_range_end or ''),
                ('range_size', str(self.range_size or 0)),
                ('dp_bits', str(self.dp_bits or 0)),
                ('pubkey', self.pubkey or ''),
                ('solved', str(self.solved).lower()),
                ('solution', self.solution or '')
            ]
            
            for key, value in state_data:
                conn.execute('INSERT OR REPLACE INTO server_state (key, value) VALUES (?, ?)', 
                           (key, value))
                           
    def configure_search(self, start_range: str, end_range: str, pubkey: str, dp_bits: int, range_size_hex: str):
        """Configure the search parameters"""
        with self.lock:
            if self.solved:
                return False, "Search already solved"
                
            self.current_range_start = start_range
            self.current_range_end = end_range
            self.pubkey = pubkey
            self.dp_bits = dp_bits
            self.range_size = int(range_size_hex, 16)
            
            # Calculate bit range
            import math
            self.bit_range = int(math.log2(int(end_range, 16) - int(start_range, 16))) + 1
            
            # Initialize collision solver
            self.collision_solver = CollisionSolver(pubkey, start_range, self.bit_range)
            
            self._save_state()
            self._generate_work_ranges()
            
            logger.info(f"Search configured: {start_range} to {end_range}, pubkey: {pubkey}, dp_bits: {dp_bits}")
            return True, "Search configured successfully"
            
    def _generate_work_ranges(self):
        """Generate work ranges for clients"""
        if not all([self.current_range_start, self.current_range_end, self.range_size]):
            return
            
        start = int(self.current_range_start, 16)
        end = int(self.current_range_end, 16)
        chunk_size = self.range_size
        
        with sqlite3.connect(self.db_path) as conn:
            # Clear existing pending ranges
            conn.execute('DELETE FROM work_ranges WHERE status = "pending"')
            
            current = start
            range_id = 0
            
            while current < end:
                chunk_end = min(current + chunk_size, end)
                range_id_str = f"range_{range_id:06d}"
                
                conn.execute('''
                    INSERT OR REPLACE INTO work_ranges 
                    (range_id, start_hex, end_hex, bit_range, dp_bits, status)
                    VALUES (?, ?, ?, ?, ?, "pending")
                ''', (range_id_str, hex(current), hex(chunk_end), self.bit_range, self.dp_bits))
                
                current = chunk_end
                range_id += 1
                
            logger.info(f"Generated {range_id} work ranges")
            
    def get_work(self, client_id: str) -> Optional[Dict]:
        """Assign work range to a client"""
        with self.lock:
            if self.solved:
                return None
                
            with sqlite3.connect(self.db_path) as conn:
                # Find pending work
                cursor = conn.execute('''
                    SELECT range_id, start_hex, end_hex, bit_range, dp_bits 
                    FROM work_ranges 
                    WHERE status = "pending" 
                    ORDER BY range_id 
                    LIMIT 1
                ''')
                
                row = cursor.fetchone()
                if not row:
                    return None
                    
                range_id, start_hex, end_hex, bit_range, dp_bits = row
                
                # Assign to client
                conn.execute('''
                    UPDATE work_ranges 
                    SET assigned_to = ?, assigned_at = ?, status = "assigned"
                    WHERE range_id = ?
                ''', (client_id, time.time(), range_id))
                
                work = {
                    'range_id': range_id,
                    'start_range': start_hex,
                    'end_range': end_hex,
                    'bit_range': bit_range,
                    'dp_bits': dp_bits,
                    'pubkey': self.pubkey
                }
                
                logger.info(f"Assigned work {range_id} to client {client_id}")
                return work
                
    def submit_points(self, client_id: str, points: List[Dict]) -> Dict:
        """Submit distinguished points from a client"""
        with self.lock:
            if self.solved:
                return {'status': 'solved', 'solution': self.solution}
                
            collision_found = False
            solution = None
            
            with sqlite3.connect(self.db_path) as conn:
                for point in points:
                    x_coord = point['x_coord']
                    distance = point['distance']
                    kang_type = point['kang_type']
                    
                    # Check for collision
                    cursor = conn.execute('''
                        SELECT distance, kang_type FROM distinguished_points 
                        WHERE x_coord = ?
                    ''', (x_coord,))
                    
                    existing = cursor.fetchone()
                    if existing:
                        existing_distance, existing_type = existing
                        
                        # Check if it's a valid collision (different types or different distances)
                        if (existing_type != kang_type or 
                            (existing_type == kang_type and existing_type != 0 and existing_distance != distance)):
                            
                            collision_found = True
                            solution = self._solve_collision(x_coord, distance, kang_type, 
                                                          existing_distance, existing_type)
                            if solution:
                                self.solved = True
                                self.solution = solution
                                self._save_state()
                                logger.info(f"COLLISION FOUND! Solution: {solution}")
                                break
                    else:
                        # Add new point
                        try:
                            conn.execute('''
                                INSERT INTO distinguished_points 
                                (x_coord, distance, kang_type, client_id, found_at)
                                VALUES (?, ?, ?, ?, ?)
                            ''', (x_coord, distance, kang_type, client_id, time.time()))
                        except sqlite3.IntegrityError:
                            # Point already exists (race condition)
                            pass
                            
            if collision_found:
                return {'status': 'solved', 'solution': solution}
            else:
                return {'status': 'success', 'points_processed': len(points)}
                
    def _solve_collision(self, x_coord: str, dist1: str, type1: int, dist2: str, type2: int) -> Optional[str]:
        """Solve collision using SOTA method"""
        if not self.collision_solver:
            logger.error("Collision solver not initialized")
            return None
            
        logger.info(f"Collision detected at x={x_coord[:16]}..., types={type1},{type2}")
        
        # Get collision summary for logging
        collision_info = self.collision_solver.get_collision_summary(x_coord, dist1, type1, dist2, type2)
        logger.info(f"Collision details: {collision_info}")
        
        # Attempt to solve the collision
        solution = self.collision_solver.solve_collision(x_coord, dist1, type1, dist2, type2)
        
        if solution:
            logger.info(f"Successfully solved collision: {solution}")
            return solution
        else:
            logger.info("Collision could not be solved or was invalid")
            return None
        
    def get_status(self) -> Dict:
        """Get server status"""
        with self.lock:
            with sqlite3.connect(self.db_path) as conn:
                # Count DPs
                cursor = conn.execute('SELECT COUNT(*) FROM distinguished_points')
                dp_count = cursor.fetchone()[0]
                
                # Count work ranges by status
                cursor = conn.execute('''
                    SELECT status, COUNT(*) FROM work_ranges 
                    GROUP BY status
                ''')
                work_status = dict(cursor.fetchall())
                
                return {
                    'solved': self.solved,
                    'solution': self.solution,
                    'dp_count': dp_count,
                    'work_ranges': work_status,
                    'search_range': {
                        'start': self.current_range_start,
                        'end': self.current_range_end,
                        'pubkey': self.pubkey,
                        'dp_bits': self.dp_bits
                    }
                }

# Flask app
app = Flask(__name__)
server = None

@app.route('/api/configure', methods=['POST'])
def configure():
    """Configure search parameters"""
    data = request.json
    success, message = server.configure_search(
        data['start_range'], 
        data['end_range'], 
        data['pubkey'], 
        data['dp_bits'],
        data['range_size']
    )
    return jsonify({'success': success, 'message': message})

@app.route('/api/get_work', methods=['POST'])
def get_work():
    """Get work assignment for client"""
    data = request.json
    client_id = data['client_id']
    work = server.get_work(client_id)
    if work:
        return jsonify({'success': True, 'work': work})
    else:
        return jsonify({'success': False, 'message': 'No work available'})

@app.route('/api/submit_points', methods=['POST'])
def submit_points():
    """Submit distinguished points"""
    data = request.json
    client_id = data['client_id']
    points = data['points']
    
    result = server.submit_points(client_id, points)
    return jsonify(result)

@app.route('/api/status', methods=['GET'])
def status():
    """Get server status"""
    return jsonify(server.get_status())

@app.route('/', methods=['GET'])
def index():
    """Server info page"""
    status = server.get_status()
    return f"""
    <html><head><title>RCKangaroo Server</title></head>
    <body>
    <h1>RCKangaroo Distributed Server</h1>
    <h2>Status</h2>
    <p><strong>Solved:</strong> {status['solved']}</p>
    <p><strong>Solution:</strong> {status.get('solution', 'None')}</p>
    <p><strong>Distinguished Points:</strong> {status['dp_count']}</p>
    <p><strong>Work Ranges:</strong> {status['work_ranges']}</p>
    <h2>Search Configuration</h2>
    <p><strong>Range:</strong> {status['search_range']['start']} to {status['search_range']['end']}</p>
    <p><strong>Public Key:</strong> {status['search_range']['pubkey']}</p>
    <p><strong>DP Bits:</strong> {status['search_range']['dp_bits']}</p>
    </body></html>
    """

def main():
    parser = argparse.ArgumentParser(description='RCKangaroo Distributed Server')
    parser.add_argument('--host', default='0.0.0.0', help='Server host')
    parser.add_argument('--port', type=int, default=8080, help='Server port') 
    parser.add_argument('--db', default='kangaroo_server.db', help='Database path')
    args = parser.parse_args()
    
    global server
    server = KangarooServer(args.db)
    
    logger.info(f"Starting RCKangaroo Server on {args.host}:{args.port}")
    app.run(host=args.host, port=args.port, threaded=True)

if __name__ == '__main__':
    main()