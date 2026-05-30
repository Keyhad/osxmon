'use strict';

'use client';

import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Gauge } from '@/components/Gauge';
import { HistoryChart } from '@/components/HistoryChart';
import { ProcessTable } from '@/components/ProcessTable';
import { ConfigPanel, Config } from '@/components/ConfigPanel';
import styles from './page.module.css';

const API_BASE = 'http://localhost:8000';

interface CpuCore {
  id: number;
  user: number;
  system: number;
  idle: number;
  load: number;
}

interface CpuMetrics {
  user: number;
  system: number;
  idle: number;
  temperature?: number;
  cores?: CpuCore[];
}

interface MemoryMetrics {
  total: number;
  free: number;
  used: number;
  active: number;
  inactive: number;
  wired: number;
  compressed: number;
}

interface DiskMetrics {
  mountPoint: string;
  total: number;
  used: number;
  free: number;
  usedPercent: number;
}

interface NetworkMetrics {
  interface: string;
  inputBytes: number;
  outputBytes: number;
  inputSpeed: number;
  outputSpeed: number;
}

interface Process {
  pid: number;
  name: string;
  cpuPercent: number;
  residentMemory: number;
}

interface MetricsResponse {
  cpu?: CpuMetrics;
  memory?: MemoryMetrics;
  disks?: DiskMetrics[];
  network?: NetworkMetrics[];
  processes?: Process[];
  timestamp: number;
}

export default function Dashboard() {
  const [config, setConfig] = useState<Config>({
    pollingIntervalMs: 1000,
    enableCpu: true,
    enableMemory: true,
    enableDisk: true,
    enableNetwork: true,
    enableProcesses: true,
    maxProcesses: 10,
  });

  const [metrics, setMetrics] = useState<MetricsResponse | null>(null);
  const [isConnected, setIsConnected] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  
  // Historical telemetry trends (capped at 30 entries)
  const [cpuHistory, setCpuHistory] = useState<number[]>([]);
  const [memoryHistory, setMemoryHistory] = useState<number[]>([]);
  
  const pollingTimerRef = useRef<NodeJS.Timeout | null>(null);
  const reconnectTimerRef = useRef<NodeJS.Timeout | null>(null);

  // Helper to format byte counts to readable speeds (e.g. MB/s, KB/s)
  const formatSpeed = (bytesPerSec: number) => {
    if (bytesPerSec < 1024) return `${bytesPerSec.toFixed(0)} B/s`;
    const kb = bytesPerSec / 1024;
    if (kb < 1024) return `${kb.toFixed(1)} KB/s`;
    const mb = kb / 1024;
    return `${mb.toFixed(1)} MB/s`;
  };

  const formatDiskSize = (bytes: number) => {
    const gb = bytes / (1024 * 1024 * 1024);
    return `${gb.toFixed(1)} GB`;
  };

  // Fetch current backend configuration on mount
  const fetchConfig = async () => {
    try {
      const res = await fetch(`${API_BASE}/api/config`);
      if (res.ok) {
        const data = await res.json();
        setConfig(data);
        setIsConnected(true);
      }
    } catch (err) {
      console.warn('Failed to fetch initial configuration. Server might be offline.', err);
      setIsConnected(false);
    }
  };

  // Push local configuration changes to the backend
  const updateConfig = async (newConfig: Config) => {
    setConfig(newConfig);
    if (!isConnected) return;
    try {
      const res = await fetch(`${API_BASE}/api/config`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(newConfig),
      });
      if (res.ok) {
        const savedConfig = await res.json();
        setConfig(savedConfig);
      }
    } catch (err) {
      console.error('Failed to update config on server:', err);
    }
  };

  // Pull system metrics
  const fetchMetrics = useCallback(async () => {
    if (isPaused) return;
    try {
      const res = await fetch(`${API_BASE}/api/metrics`);
      if (res.ok) {
        const data: MetricsResponse = await res.json();
        setMetrics(data);
        setIsConnected(true);

        // Update historical trends
        if (data.cpu) {
          const totalCpuLoad = data.cpu.user + data.cpu.system;
          setCpuHistory((prev) => [...prev.slice(-29), totalCpuLoad]);
        }
        if (data.memory) {
          const memLoadPercent = (data.memory.used / data.memory.total) * 100;
          setMemoryHistory((prev) => [...prev.slice(-29), memLoadPercent]);
        }
      }
    } catch (err) {
      console.warn('Metrics collection poll failed. Server disconnected.', err);
      setIsConnected(false);
    }
  }, [isPaused]);

  // Main polling setup
  useEffect(() => {
    if (pollingTimerRef.current) clearInterval(pollingTimerRef.current);

    if (isConnected && !isPaused) {
      pollingTimerRef.current = setInterval(fetchMetrics, config.pollingIntervalMs);
      // Run immediate poll
      // eslint-disable-next-line react-hooks/set-state-in-effect
      fetchMetrics();
    }

    return () => {
      if (pollingTimerRef.current) clearInterval(pollingTimerRef.current);
    };
  }, [isConnected, isPaused, config.pollingIntervalMs, fetchMetrics]);

  // Initial load and connection retry loop
  useEffect(() => {
    // eslint-disable-next-line react-hooks/set-state-in-effect
    fetchConfig();

    reconnectTimerRef.current = setInterval(() => {
      if (!isConnected) {
        console.log('Attempting to reconnect to C++ backend...');
        fetchConfig();
      }
    }, 3000);

    return () => {
      if (reconnectTimerRef.current) clearInterval(reconnectTimerRef.current);
    };
  }, [isConnected]);

  return (
    <div className={styles.container}>
      {/* 1. Header Row */}
      <header className={styles.header}>
        <div className={styles.titleArea}>
          <h1>{config.title || 'osxmon dashboard'}</h1>
          <p>Native performance and system resource telemetries</p>
        </div>
        <div className={`${styles.statusIndicator} ${isConnected ? styles.online : ''}`}>
          <div className={styles.statusDot} />
          {isConnected ? 'BACKEND ONLINE (C++)' : 'BACKEND DISCONNECTED'}
        </div>
      </header>

      {!isConnected && (
        <div className={styles.offlineAlert}>
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0zM12 9v4M12 17h.01" />
          </svg>
          Unable to establish connection to C++ server on port 8000. Start the backend by running <code>./osxmon_server</code> in <code>backend/build</code>.
        </div>
      )}

      {/* 2. Main Dashboard Grid */}
      <div className={styles.grid}>
        
        {/* Left Side: Gauges, Disks, Network, Processes */}
        <div className={styles.mainCol}>
          
          {/* Gauges Row */}
          <div className={styles.gaugesRow}>
            {/* CPU Card */}
            {config.enableCpu && (
              <div className={styles.card}>
                <div className={styles.cardHeader}>
                  <span className={styles.cardTitle}>
                    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" style={{ color: 'var(--color-cpu)' }}>
                      <rect x="4" y="4" width="16" height="16" rx="2" />
                      <path d="M9 9h6v6H9zM9 1v3M15 1v3M9 20v3M15 20v3M20 9h3M20 15h3M1 9h3M1 15h3" />
                    </svg>
                    CPU Load
                  </span>
                  {metrics?.cpu?.temperature !== undefined && metrics.cpu.temperature > 0 && (
                    <span className={styles.cpuTempBadge}>
                      <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                        <path d="M14 14.76V3.5a2.5 2.5 0 0 0-5 0v11.26a4.5 4.5 0 1 0 5 0z" />
                      </svg>
                      {metrics.cpu.temperature.toFixed(1)}°C
                    </span>
                  )}
                </div>
                {metrics?.cpu ? (
                  <div className={styles.metricStack}>
                    <Gauge
                      value={metrics.cpu.user + metrics.cpu.system}
                      title="CPU"
                      subtitle={`${metrics.cpu.user.toFixed(1)}% user, ${metrics.cpu.system.toFixed(1)}% sys`}
                      colorStart="#00e5ff"
                      colorEnd="#0080ff"
                      glowColor="rgba(0, 240, 255, 0.3)"
                      icon={
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                          <path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z" />
                        </svg>
                      }
                    />
                    <div className={styles.chartWrap}>
                      <HistoryChart
                        history={cpuHistory}
                        color="var(--color-cpu)"
                        glowColor="var(--color-cpu-glow)"
                        title="cpu"
                      />
                    </div>
                    {/* Per-core load bars */}
                    {metrics.cpu.cores && metrics.cpu.cores.length > 0 && (
                      <div className={styles.coresSection}>
                        <div className={styles.coresSectionLabel}>Core Load</div>
                        <div className={styles.coresGrid}>
                          {metrics.cpu.cores.map((core) => (
                            <div key={core.id} className={styles.coreBarWrapper} title={`Core ${core.id}: ${core.load.toFixed(1)}%`}>
                              <div className={styles.coreBarTrack}>
                                <div
                                  className={styles.coreBarFill}
                                  style={{
                                    height: `${Math.max(core.load, 2)}%`,
                                    background: `linear-gradient(to top, #0080ff, #00e5ff)`,
                                    boxShadow: core.load > 50 ? '0 0 6px rgba(0,229,255,0.4)' : 'none',
                                  }}
                                />
                              </div>
                              <span className={styles.coreBarLabel}>C{core.id}</span>
                            </div>
                          ))}
                        </div>
                      </div>
                    )}
                  </div>
                ) : (
                  <div className={styles.emptyState}>
                    Awaiting telemetry response...
                  </div>
                )}
              </div>
            )}

            {/* Memory Card */}
            {config.enableMemory && (
              <div className={styles.card}>
                <div className={styles.cardHeader}>
                  <span className={styles.cardTitle}>
                    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" style={{ color: 'var(--color-memory)' }}>
                      <path d="M6 19h12a2 2 0 0 0 2-2V7a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2zM10 5v4M14 5v4M10 15v4M14 15v4" />
                    </svg>
                    Physical Memory
                  </span>
                </div>
                {metrics?.memory ? (
                  <div className={styles.metricStack}>
                    <Gauge
                      value={(metrics.memory.used / metrics.memory.total) * 100}
                      title="RAM"
                      subtitle={`${formatDiskSize(metrics.memory.used)} of ${formatDiskSize(metrics.memory.total)}`}
                      colorStart="#d946ef"
                      colorEnd="#8b5cf6"
                      glowColor="rgba(189, 92, 255, 0.3)"
                      icon={
                        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                          <path d="M22 12h-4l-3 9L9 3l-3 9H2" />
                        </svg>
                      }
                    />
                    <div className={styles.chartWrap}>
                      <HistoryChart
                        history={memoryHistory}
                        color="var(--color-memory)"
                        glowColor="var(--color-memory-glow)"
                        title="memory"
                      />
                    </div>
                  </div>
                ) : (
                  <div className={styles.emptyState}>
                    Awaiting telemetry response...
                  </div>
                )}
              </div>
            )}
          </div>

          {/* Disks & Network Row */}
          <div className={styles.secondaryRow}>
            
            {/* Storage Card */}
            {config.enableDisk && (
              <div className={styles.card} style={{ gridColumn: 'span 1' }}>
                <div className={styles.cardHeader}>
                  <span className={styles.cardTitle}>
                    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" style={{ color: 'var(--color-disk)' }}>
                      <rect x="2" y="2" width="20" height="8" rx="2" ry="2" />
                      <rect x="2" y="14" width="20" height="8" rx="2" ry="2" />
                      <line x1="6" y1="6" x2="6.01" y2="6" />
                      <line x1="6" y1="18" x2="6.01" y2="18" />
                    </svg>
                    Disk Storage
                  </span>
                </div>
                {metrics?.disks && metrics.disks.length > 0 ? (
                  <div className={styles.diskList}>
                    {metrics.disks.map((disk) => (
                      <div key={disk.mountPoint} className={styles.diskItem}>
                        <div className={styles.diskInfo}>
                          <span className={styles.mountLabel}>{disk.mountPoint}</span>
                          <span className={styles.spaceLabel}>
                            {formatDiskSize(disk.used)} / {formatDiskSize(disk.total)} ({disk.usedPercent.toFixed(0)}%)
                          </span>
                        </div>
                        <div className={styles.progressBarTrack}>
                          <div 
                            className={styles.progressBarFill} 
                            style={{ 
                              width: `${disk.usedPercent}%`, 
                              backgroundColor: 'var(--color-disk)',
                              boxShadow: '0 0 8px var(--color-disk-glow)'
                            }} 
                          />
                        </div>
                      </div>
                    ))}
                  </div>
                ) : (
                  <div className={styles.emptyState}>
                    Awaiting telemetry response...
                  </div>
                )}
              </div>
            )}

            {/* Network Card */}
            {config.enableNetwork && (
              <div className={styles.card} style={{ gridColumn: 'span 1' }}>
                <div className={styles.cardHeader}>
                  <span className={styles.cardTitle}>
                    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" style={{ color: 'var(--color-network)' }}>
                      <polyline points="22 12 18 12 15 21 9 3 6 12 2 12" />
                    </svg>
                    Network interfaces
                  </span>
                </div>
                {metrics?.network && metrics.network.length > 0 ? (
                  <div className={styles.netList}>
                    {metrics.network.map((net) => (
                      <div key={net.interface} className={styles.netItem}>
                        <span className={styles.netName}>{net.interface}</span>
                        
                        {/* Download */}
                        <span className={`${styles.netSpeed} ${styles.netSpeedDown}`}>
                          <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                            <line x1="12" y1="5" x2="12" y2="19" />
                            <polyline points="19 12 12 19 5 12" />
                          </svg>
                          {formatSpeed(net.inputSpeed)}
                        </span>
  
                        {/* Upload */}
                        <span className={`${styles.netSpeed} ${styles.netSpeedUp}`}>
                          <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                            <line x1="12" y1="19" x2="12" y2="5" />
                            <polyline points="5 12 12 5 19 12" />
                          </svg>
                          {formatSpeed(net.outputSpeed)}
                        </span>
                      </div>
                    ))}
                  </div>
                ) : (
                  <div className={styles.emptyState}>
                    Awaiting telemetry response...
                  </div>
                )}
              </div>
            )}

          </div>

          {/* Processes Card */}
          {config.enableProcesses && (
            <div className={styles.card}>
              {metrics?.processes ? (
                <ProcessTable processes={metrics.processes} />
              ) : (
                <div className={styles.emptyState}>
                  Awaiting telemetry response...
                </div>
              )}
            </div>
          )}

        </div>

        {/* Right Side: Configuration Sidebar */}
        <div className={styles.sidebarCol}>
          <div className={styles.card}>
            <div className={styles.cardHeader}>
              <span className={styles.cardTitle}>
                <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                  <circle cx="12" cy="12" r="3" />
                  <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z" />
                </svg>
                Controls & Tuning
              </span>
            </div>
            <ConfigPanel
              config={config}
              onChange={updateConfig}
              isPaused={isPaused}
              onPauseToggle={() => setIsPaused(!isPaused)}
            />
          </div>
        </div>

      </div>
    </div>
  );
}
