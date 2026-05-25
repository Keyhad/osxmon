import React from 'react';

interface HistoryChartProps {
  history: number[]; // Array of values (0 to 100)
  color: string;
  glowColor: string;
  title: string;
}

export const HistoryChart: React.FC<HistoryChartProps> = ({
  history = [],
  color,
  glowColor,
  title,
}) => {
  const width = 240;
  const height = 45;
  const padding = 2;

  if (history.length < 2) {
    return <div style={{ height, width, opacity: 0.2 }} />;
  }

  // Calculate SVG points
  const points = history.map((val, idx) => {
    const x = (idx / (history.length - 1)) * (width - padding * 2) + padding;
    const y = height - (val / 100) * (height - padding * 2) - padding;
    return { x, y };
  });

  const pathD = points.reduce(
    (acc, pt, idx) => (idx === 0 ? `M ${pt.x} ${pt.y}` : `${acc} L ${pt.x} ${pt.y}`),
    ''
  );

  // Closed path for filled gradient area
  const fillD = `${pathD} L ${points[points.length - 1].x} ${height} L ${points[0].x} ${height} Z`;

  return (
    <svg 
      width="100%" 
      height={height} 
      viewBox={`0 0 ${width} ${height}`} 
      preserveAspectRatio="none"
      style={{ overflow: 'visible', filter: `drop-shadow(0 2px 4px rgba(0,0,0,0.15))` }}
    >
      <defs>
        <linearGradient id={`chart-grad-${title}`} x1="0%" y1="0%" x2="0%" y2="100%">
          <stop offset="0%" stopColor={color} stopOpacity={0.25} />
          <stop offset="100%" stopColor={color} stopOpacity={0.0} />
        </linearGradient>
      </defs>
      
      {/* Filled Area */}
      <path d={fillD} fill={`url(#chart-grad-${title})`} />
      
      {/* Sparkline Path */}
      <path
        d={pathD}
        fill="none"
        stroke={color}
        strokeWidth={2}
        strokeLinecap="round"
        strokeLinejoin="round"
        style={{ filter: `drop-shadow(0 0 3px ${glowColor})` }}
      />
      
      {/* End pulse dot */}
      {points.length > 0 && (
        <circle
          cx={points[points.length - 1].x}
          cy={points[points.length - 1].y}
          r={3}
          fill={color}
          style={{ filter: `drop-shadow(0 0 4px ${color})` }}
        />
      )}
    </svg>
  );
};
