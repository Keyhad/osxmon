import React from 'react';
import styles from './Gauge.module.css';

interface GaugeProps {
  value: number; // 0 to 100
  title: string;
  subtitle?: string;
  colorStart: string;
  colorEnd: string;
  glowColor: string;
  icon?: React.ReactNode;
}

export const Gauge: React.FC<GaugeProps> = ({
  value = 0,
  title,
  subtitle,
  colorStart,
  colorEnd,
  glowColor,
  icon,
}) => {
  const radius = 52;
  const strokeWidth = 8;
  const circumference = 2 * Math.PI * radius;
  const arcLength = circumference * 0.75; // 270 degrees
  const clampedValue = Math.min(Math.max(value, 0), 100);
  const strokeDashoffset = arcLength - (clampedValue / 100) * arcLength;

  return (
    <div className={styles.gaugeContainer}>
      <svg 
        width={140} 
        height={140} 
        viewBox="0 0 140 140" 
        className={styles.gaugeSvg}
        style={{ '--glow-color': glowColor } as React.CSSProperties}
      >
        <defs>
          <linearGradient id={`grad-${title}`} x1="0%" y1="90%" x2="100%" y2="0%">
            <stop offset="0%" stopColor={colorStart} />
            <stop offset="100%" stopColor={colorEnd} />
          </linearGradient>
        </defs>
        {/* Background track (semi-circle arc) */}
        <circle
          cx={70}
          cy={70}
          r={radius}
          fill="none"
          stroke="rgba(255, 255, 255, 0.04)"
          strokeWidth={strokeWidth}
          strokeDasharray={`${arcLength} ${circumference}`}
          strokeLinecap="round"
          transform="rotate(135 70 70)"
        />
        {/* Active progress (semi-circle arc) */}
        <circle
          cx={70}
          cy={70}
          r={radius}
          fill="none"
          stroke={`url(#grad-${title})`}
          strokeWidth={strokeWidth}
          strokeDasharray={`${arcLength} ${circumference}`}
          strokeDashoffset={strokeDashoffset}
          strokeLinecap="round"
          transform="rotate(135 70 70)"
          className={styles.progressCircle}
        />
      </svg>
      <div className={styles.gaugeContent}>
        {icon && <div className={styles.gaugeIcon}>{icon}</div>}
        <span className={styles.gaugeValue}>
          {typeof value === 'number' && !isNaN(value) ? value.toFixed(value < 10 ? 1 : 0) : '0'}%
        </span>
        <span className={styles.gaugeTitle}>{title}</span>
        {subtitle && <span className={styles.gaugeSubtitle} title={subtitle}>{subtitle}</span>}
      </div>
    </div>
  );
};
