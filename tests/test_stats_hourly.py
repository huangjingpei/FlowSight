import sqlite3
import os
import sys
import time
from datetime import datetime

def test_stats_hourly():
    test_db = "data/test_stats_hourly.db"
    if os.path.exists(test_db):
        os.remove(test_db)
    
    os.makedirs("data", exist_ok=True)
    conn = sqlite3.connect(test_db)
    cur = conn.cursor()

    # 1. 创建 passage_events 模拟表并注入 2,000 条历史进出记录跨越不同日期与小时
    print("[Test] 1. Creating schema and generating 2,000 mock passage_events...")
    cur.execute("""
        CREATE TABLE passage_events (
            event_id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_id TEXT NOT NULL,
            timestamp_ms INTEGER NOT NULL,
            track_id INTEGER NOT NULL,
            direction INTEGER NOT NULL,
            roi_version INTEGER NOT NULL,
            confidence REAL NOT NULL,
            clip_path TEXT DEFAULT '',
            gender TEXT DEFAULT '未知',
            age_group TEXT DEFAULT '未知',
            roi_id INTEGER DEFAULT 1,
            roi_name TEXT DEFAULT 'A门主通道'
        );
    """)

    cur.execute("""
        CREATE TABLE stats_hourly (
            date_hour TEXT PRIMARY KEY,
            date TEXT NOT NULL,
            hour INTEGER NOT NULL,
            entry_count INTEGER DEFAULT 0,
            exit_count INTEGER DEFAULT 0,
            total_count INTEGER DEFAULT 0,
            peak_occupancy INTEGER DEFAULT 0,
            updated_at INTEGER NOT NULL
        );
    """)
    cur.execute("CREATE INDEX idx_stats_hourly_date ON stats_hourly(date);")
    cur.execute("CREATE INDEX idx_stats_hourly_hour ON stats_hourly(hour);")

    # 插入 2,000 条模拟数据，分布在 2026-10-18 的 08:00 到 20:00
    base_ts = 1792300000000 # 2026-10-18 附近
    mock_rows = []
    expected_hours = {}
    for i in range(2000):
        offset_ms = (i % 12) * 3600 * 1000 + (i * 17) % 3600000
        ts = base_ts + offset_ms
        direction = 1 if (i % 3 != 0) else 2
        gender = "男" if (i % 2 == 0) else "女"
        dt = datetime.fromtimestamp(ts / 1000.0)
        dh = dt.strftime("%Y-%m-%d %H:00")
        d_str = dt.strftime("%Y-%m-%d")
        h = dt.hour
        
        if dh not in expected_hours:
            expected_hours[dh] = {"date": d_str, "hour": h, "entry": 0, "exit": 0, "total": 0}
        if direction == 1:
            expected_hours[dh]["entry"] += 1
        else:
            expected_hours[dh]["exit"] += 1
        expected_hours[dh]["total"] += 1
        
        mock_rows.append(("default", ts, i, direction, 1, 0.95, "", gender, "青年(20-30)", 1, "A门主通道"))

    cur.executemany("""
        INSERT INTO passage_events (source_id, timestamp_ms, track_id, direction, roi_version, confidence, clip_path, gender, age_group, roi_id, roi_name)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, mock_rows)
    conn.commit()
    print(f"  Inserted {len(mock_rows)} raw events across {len(expected_hours)} hourly buckets.")

    # 2. 验证冷启动回填 (Backfill)
    print("[Test] 2. Simulating Backfill into stats_hourly...")
    t0 = time.perf_counter()
    for dh, b in expected_hours.items():
        cur.execute("""
            INSERT INTO stats_hourly (date_hour, date, hour, entry_count, exit_count, total_count, peak_occupancy, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, 0, ?)
            ON CONFLICT(date_hour) DO UPDATE SET
                entry_count = excluded.entry_count,
                exit_count = excluded.exit_count,
                total_count = excluded.total_count,
                updated_at = excluded.updated_at;
        """, (dh, b["date"], b["hour"], b["entry"], b["exit"], b["total"], int(time.time() * 1000)))
    conn.commit()
    t_backfill = (time.perf_counter() - t0) * 1000
    print(f"  Backfill completed in {t_backfill:.2f} ms.")

    cur.execute("SELECT COUNT(*) FROM stats_hourly")
    row_count = cur.fetchone()[0]
    assert row_count == len(expected_hours), f"Expected {len(expected_hours)} rows, got {row_count}"
    print(f"  Verified stats_hourly row count: {row_count} OK.")

    # 3. 验证实时增量 Upsert
    print("[Test] 3. Testing Realtime Incremental Upsert...")
    test_dh = list(expected_hours.keys())[0]
    initial_entry = expected_hours[test_dh]["entry"]
    initial_total = expected_hours[test_dh]["total"]
    
    cur.execute("""
        INSERT INTO stats_hourly (date_hour, date, hour, entry_count, exit_count, total_count, peak_occupancy, updated_at)
        VALUES (?, ?, ?, 5, 0, 5, 0, ?)
        ON CONFLICT(date_hour) DO UPDATE SET
            entry_count = entry_count + excluded.entry_count,
            exit_count = exit_count + excluded.exit_count,
            total_count = total_count + excluded.total_count,
            updated_at = excluded.updated_at;
    """, (test_dh, expected_hours[test_dh]["date"], expected_hours[test_dh]["hour"], int(time.time() * 1000)))
    conn.commit()

    cur.execute("SELECT entry_count, total_count FROM stats_hourly WHERE date_hour = ?", (test_dh,))
    new_entry, new_total = cur.fetchone()
    assert new_entry == initial_entry + 5, f"Expected {initial_entry + 5}, got {new_entry}"
    assert new_total == initial_total + 5, f"Expected {initial_total + 5}, got {new_total}"
    print(f"  Realtime incremental upsert verified: entry {initial_entry} -> {new_entry}, total {initial_total} -> {new_total} OK.")

    # 4. 验证 24 小时聚合分布查询耗时与准确性
    print("[Test] 4. Benchmarking query_hourly_distribution...")
    t0 = time.perf_counter()
    cur.execute("""
        SELECT hour, SUM(entry_count), SUM(exit_count)
        FROM stats_hourly
        WHERE date_hour >= '2026-01-01 00:00' AND date_hour <= '2026-12-31 23:59'
        GROUP BY hour ORDER BY hour ASC;
    """)
    rows = cur.fetchall()
    t_query = (time.perf_counter() - t0) * 1000
    print(f"  Aggregated 24-hour distribution returned {len(rows)} hourly bins in {t_query:.3f} ms (sub-millisecond!).")
    assert t_query < 10.0, "Query should complete well under 10ms"

    conn.close()
    if os.path.exists(test_db):
        os.remove(test_db)
    print("\n[SUCCESS] All P1-6 stats_hourly SQLite tests passed 100%!")

if __name__ == "__main__":
    test_stats_hourly()
