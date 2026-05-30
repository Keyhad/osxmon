import React, { useState, useMemo } from 'react';
import styles from './ProcessTable.module.css';

interface Process {
  pid: number;
  name: string;
  cpuPercent: number;
  residentMemory: number;
}

interface ProcessTableProps {
  processes: Process[];
}

type SortKey = 'pid' | 'name' | 'cpuPercent' | 'residentMemory';
type SortOrder = 'asc' | 'desc';

export const ProcessTable: React.FC<ProcessTableProps> = ({ processes = [] }) => {
  const [searchTerm, setSearchTerm] = useState('');
  const [sortKey, setSortKey] = useState<SortKey>('cpuPercent');
  const [sortOrder, setSortOrder] = useState<SortOrder>('desc');

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  };

  const handleSort = (key: SortKey) => {
    if (sortKey === key) {
      setSortOrder(sortOrder === 'asc' ? 'desc' : 'asc');
    } else {
      setSortKey(key);
      setSortOrder('desc'); // Default to descending when changing sort
    }
  };

  const filteredAndSortedProcesses = useMemo(() => {
    // 1. Filter
    const result = processes.filter(
      (p) =>
        p.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        p.pid.toString().includes(searchTerm)
    );

    // 2. Sort
    result.sort((a, b) => {
      const valA = a[sortKey];
      const valB = b[sortKey];

      if (typeof valA === 'string' && typeof valB === 'string') {
        return sortOrder === 'asc' 
          ? valA.localeCompare(valB) 
          : valB.localeCompare(valA);
      }

      // Numeric comparison
      return sortOrder === 'asc'
        ? (valA as number) - (valB as number)
        : (valB as number) - (valA as number);
    });

    return result;
  }, [processes, searchTerm, sortKey, sortOrder]);

  return (
    <div className={styles.tableContainer}>
      <div className={styles.tableHeader}>
        <div className={styles.tableTitle}>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
            <rect x="3" y="3" width="18" height="18" rx="2" />
            <path d="M7 8h10M7 12h10M7 16h6" />
          </svg>
          Active Processes
        </div>
        <div className={styles.searchWrapper}>
          <input
            type="text"
            className={styles.searchInput}
            placeholder="Search process name or PID..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
          />
        </div>
      </div>

      <div style={{ overflowX: 'auto', width: '100%' }}>
        <table className={styles.processTable}>
          <thead>
            <tr>
              <th onClick={() => handleSort('pid')}>
                PID {sortKey === 'pid' && (sortOrder === 'asc' ? '▲' : '▼')}
              </th>
              <th onClick={() => handleSort('name')}>
                Process Name {sortKey === 'name' && (sortOrder === 'asc' ? '▲' : '▼')}
              </th>
              <th onClick={() => handleSort('cpuPercent')} style={{ textAlign: 'right' }}>
                CPU % {sortKey === 'cpuPercent' && (sortOrder === 'asc' ? '▲' : '▼')}
              </th>
              <th onClick={() => handleSort('residentMemory')} style={{ textAlign: 'right' }}>
                Memory {sortKey === 'residentMemory' && (sortOrder === 'asc' ? '▲' : '▼')}
              </th>
            </tr>
          </thead>
          <tbody>
            {filteredAndSortedProcesses.length > 0 ? (
              filteredAndSortedProcesses.map((proc) => (
                <tr key={`${proc.pid}-${proc.name}`} className={styles.processRow}>
                  <td className={styles.pidCell}>{proc.pid}</td>
                  <td className={styles.nameCell} title={proc.name}>
                    {proc.name}
                  </td>
                  <td className={styles.cpuCell} style={{ textAlign: 'right' }}>
                    {proc.cpuPercent.toFixed(1)}%
                  </td>
                  <td className={styles.memCell} style={{ textAlign: 'right' }}>
                    {formatBytes(proc.residentMemory)}
                  </td>
                </tr>
              ))
            ) : (
              <tr>
                <td colSpan={4} className={styles.noResults}>
                  No active processes found matching search filter.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
};
