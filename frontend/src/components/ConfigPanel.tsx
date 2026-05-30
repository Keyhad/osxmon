import React from 'react';
import styles from './ConfigPanel.module.css';

export interface Config {
  title?: string;
  pollingIntervalMs: number;
  enableCpu: boolean;
  enableMemory: boolean;
  enableDisk: boolean;
  enableNetwork: boolean;
  enableProcesses: boolean;
  maxProcesses: number;
  monitoredProcesses?: string[];
}

interface ConfigPanelProps {
  config: Config;
  onChange: (newConfig: Config) => void;
  isPaused: boolean;
  onPauseToggle: () => void;
}

export const ConfigPanel: React.FC<ConfigPanelProps> = ({
  config,
  onChange,
  isPaused,
  onPauseToggle,
}) => {
  const handleToggle = (key: keyof Omit<Config, 'pollingIntervalMs' | 'maxProcesses'>) => {
    onChange({
      ...config,
      [key]: !config[key],
    });
  };

  const handleIntervalChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const val = parseInt(e.target.value, 10);
    onChange({
      ...config,
      pollingIntervalMs: val,
    });
  };

  const handleMaxProcChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const val = parseInt(e.target.value, 10);
    onChange({
      ...config,
      maxProcesses: val,
    });
  };

  return (
    <div className={styles.panel}>
      {/* 1. Polling Control */}
      <div className={styles.section}>
        <div className={styles.sectionTitle}>Refresh & Polling</div>
        
        <button 
          className={`${styles.pauseBtn} ${isPaused ? styles.paused : ''}`}
          onClick={onPauseToggle}
        >
          {isPaused ? (
            <>
              <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
                <path d="M8 5v14l11-7z" />
              </svg>
              Resume Monitoring
            </>
          ) : (
            <>
              <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
                <path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z" />
              </svg>
              Pause (0% Server Load)
            </>
          )}
        </button>

        {!isPaused && (
          <div className={styles.settingRow}>
            <div className={styles.settingLabel}>
              <span className={styles.settingTitle}>Refresh Interval</span>
              <span className={styles.settingDesc}>How often metrics refresh</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
              <input
                type="range"
                className={styles.slider}
                min={500}
                max={10000}
                step={500}
                value={config.pollingIntervalMs}
                onChange={handleIntervalChange}
              />
              <span className={styles.sliderValue}>
                {(config.pollingIntervalMs / 1000).toFixed(1)}s
              </span>
            </div>
          </div>
        )}
      </div>

      {/* 2. Metric Toggles */}
      <div className={styles.section}>
        <div className={styles.sectionTitle}>Active Telemetry</div>
        
        {/* CPU Toggle */}
        <div className={styles.settingRow}>
          <div className={styles.settingLabel}>
            <span className={styles.settingTitle}>CPU Activity</span>
            <span className={styles.settingDesc}>User, system & idle percentages</span>
          </div>
          <label className={`${styles.switch} ${styles.switchCpu}`}>
            <input
              type="checkbox"
              checked={config.enableCpu}
              onChange={() => handleToggle('enableCpu')}
            />
            <span className={styles.sliderToggle}></span>
          </label>
        </div>

        {/* Memory Toggle */}
        <div className={styles.settingRow}>
          <div className={styles.settingLabel}>
            <span className={styles.settingTitle}>Memory Usage</span>
            <span className={styles.settingDesc}>Wired, active, inactive pages</span>
          </div>
          <label className={`${styles.switch} ${styles.switchMem}`}>
            <input
              type="checkbox"
              checked={config.enableMemory}
              onChange={() => handleToggle('enableMemory')}
            />
            <span className={styles.sliderToggle}></span>
          </label>
        </div>

        {/* Disk Toggle */}
        <div className={styles.settingRow}>
          <div className={styles.settingLabel}>
            <span className={styles.settingTitle}>Disk Capacity</span>
            <span className={styles.settingDesc}>Read available space on drives</span>
          </div>
          <label className={`${styles.switch} ${styles.switchDisk}`}>
            <input
              type="checkbox"
              checked={config.enableDisk}
              onChange={() => handleToggle('enableDisk')}
            />
            <span className={styles.sliderToggle}></span>
          </label>
        </div>

        {/* Network Toggle */}
        <div className={styles.settingRow}>
          <div className={styles.settingLabel}>
            <span className={styles.settingTitle}>Network I/O</span>
            <span className={styles.settingDesc}>Monitor speeds on active ports</span>
          </div>
          <label className={`${styles.switch} ${styles.switchNet}`}>
            <input
              type="checkbox"
              checked={config.enableNetwork}
              onChange={() => handleToggle('enableNetwork')}
            />
            <span className={styles.sliderToggle}></span>
          </label>
        </div>

        {/* Processes Toggle */}
        <div className={styles.settingRow}>
          <div className={styles.settingLabel}>
            <span className={styles.settingTitle}>Process Tracking</span>
            <span className={styles.settingDesc}>List resource-heavy processes</span>
          </div>
          <label className={`${styles.switch} ${styles.switchProc}`}>
            <input
              type="checkbox"
              checked={config.enableProcesses}
              onChange={() => handleToggle('enableProcesses')}
            />
            <span className={styles.sliderToggle}></span>
          </label>
        </div>
      </div>

      {/* 3. Processes limit configuration */}
      {config.enableProcesses && (
        <div className={styles.section}>
          <div className={styles.sectionTitle}>Process Settings</div>
          <div className={styles.settingRow}>
            <div className={styles.settingLabel}>
              <span className={styles.settingTitle}>Limit Processes</span>
              <span className={styles.settingDesc}>Max rows to list</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
              <input
                type="range"
                className={styles.slider}
                min={5}
                max={50}
                step={5}
                value={config.maxProcesses}
                onChange={handleMaxProcChange}
              />
              <span className={styles.sliderValue}>{config.maxProcesses}</span>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
