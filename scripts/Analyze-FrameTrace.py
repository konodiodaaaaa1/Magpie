#!/usr/bin/env python3
"""Analyze an r9 debug CPU trace offline. Python 3.9+, standard library only."""
import argparse
import csv
import json
import math
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path


def read_trace(path):
    meta = {'lanes': {}, 'statistics': []}
    records = []
    complete = False
    header = None
    with path.open(encoding='utf-8-sig', newline='') as file:
        for line_number, row in enumerate(csv.reader(file), 1):
            if not row:
                continue
            if complete:
                raise ValueError(f'Unexpected data after completion marker, line {line_number}')
            if row[0] == '#magpie_frame_trace':
                if row[1:] != ['1']:
                    raise ValueError('Unsupported frame trace version')
                meta['version'] = 1
            elif row[0] == '#process':
                meta['pid'] = int(row[1])
            elif row[0] == '#clock':
                meta['qpc_frequency'], meta['qpc_origin'], meta['utc_filetime_100ns'] = map(int, row[1:])
            elif row[0] == '#duration_us':
                meta['duration_us'] = float(row[1])
            elif row[0] == '#lane':
                tid, total, slow_total, capacity, slow_capacity = map(int, row[2:])
                meta['lanes'][row[1]] = dict(tid=tid, total=total, slow_total=slow_total,
                    capacity=capacity, slow_capacity=slow_capacity,
                    overwritten=max(0, total-capacity), slow_overwritten=max(0, slow_total-slow_capacity))
            elif row[0] == '#stat':
                meta['statistics'].append(dict(thread=row[1], event=row[2], count=int(row[3]),
                    total_us=float(row[4]), maximum_us=float(row[5]), over50=int(row[6]),
                    maximum_start_us=float(row[7])))
            elif row[0] == '#complete':
                complete = row[1:] == ['1']
            elif row[0].startswith('#'):
                raise ValueError(f'Unknown metadata at line {line_number}')
            elif header is None:
                header = row
                if row != ['stream', 'thread', 'event', 'start_us', 'duration_us', 'frame_id', 'a', 'b']:
                    raise ValueError('Invalid CSV header')
            else:
                if len(row) != len(header):
                    raise ValueError(f'Incomplete record at line {line_number}')
                event = dict(zip(header, row))
                for key in ['start_us', 'duration_us']:
                    event[key] = float(event[key])
                    if not math.isfinite(event[key]) or event[key] < 0:
                        raise ValueError(f'Invalid {key} at line {line_number}')
                for key in ['frame_id', 'a', 'b']:
                    event[key] = int(event[key])
                if event['stream'] not in ('recent', 'slow') or event['thread'] not in ('frontend', 'backend'):
                    raise ValueError(f'Invalid stream/thread at line {line_number}')
                records.append(event)
    if not complete or meta.get('version') != 1 or not header or 'duration_us' not in meta:
        raise ValueError('Trace export is incomplete; no trustworthy complete-session report can be produced')
    for name, lane in meta['lanes'].items():
        for stream, expected in [('recent', min(lane['total'], lane['capacity'])),
                                 ('slow', min(lane['slow_total'], lane['slow_capacity']))]:
            actual = sum(r['thread'] == name and r['stream'] == stream for r in records)
            if actual != expected:
                raise ValueError(f'{name}/{stream}: expected {expected} records, found {actual}')
    if set(meta['lanes']) != {'frontend', 'backend'}:
        raise ValueError('Missing lane metadata')
    return meta, records


def interval_summary(values):
    values = sorted(v for v in values if v > 0)
    if not values:
        return None
    slow_count = max(1, math.ceil(len(values) * .01))
    slow_mean = statistics.fmean(values[-slow_count:])
    return dict(samples=len(values), mean_ms=statistics.fmean(values)/1000,
        p99_ms=values[math.ceil(len(values)*.99)-1]/1000, maximum_ms=values[-1]/1000,
        slowest_1pct_mean_ms=slow_mean/1000, interval_low1_fps=1_000_000/slow_mean,
        over50=sum(v >= 50_000 for v in values))


def analyze(meta, rows):
    recent = [r for r in rows if r['stream'] == 'recent']
    # Slow entries are copies of recent entries until the recent ring wraps.
    unique = {(r['thread'], r['event'], r['start_us'], r['duration_us'], r['frame_id'], r['a'], r['b']): r
              for r in rows}
    all_rows = sorted(unique.values(), key=lambda r: r['start_us'])
    chains = defaultdict(list)
    for row in recent:
        if row['event'] == 'PresentGap':
            chains[str(row['b'])].append(row['duration_us'])
    summaries = {chain: interval_summary(values) for chain, values in chains.items()}
    gaps = sorted((r for r in all_rows if r['event'] == 'PresentGap' and r['duration_us'] >= 50_000),
                  key=lambda r: r['duration_us'], reverse=True)[:20]
    # Inclusive overlaps are evidence, not attribution of exclusive CPU or GPU cost.
    stage_names = {'WgcAcquire', 'WgcClose', 'WgcStart', 'DuplicateReadback', 'NvofSubmit',
                   'NativeEffect', 'PublicationTransaction', 'PublicationFence', 'FocusProbe',
                   'CursorUpdate', 'Present', 'PresentGpuWait', 'BackendMessages', 'FrontendPrepare', 'FrontendMessage'}
    for gap in gaps:
        begin, end = gap['start_us'], gap['start_us'] + gap['duration_us']
        nearby = [r for r in all_rows if r['event'] in stage_names and r['duration_us'] >= 5_000
                  and r['start_us'] < end and r['start_us'] + r['duration_us'] > begin]
        gap['overlapping_stages'] = [dict(event=r['event'], thread=r['thread'],
                start_us=r['start_us'], duration_us=r['duration_us'])
            for r in sorted(nearby, key=lambda r: r['duration_us'], reverse=True)[:5]]
        in_gap = [r for r in recent if begin <= r['start_us'] <= end]
        gap['recent_events'] = dict(Counter(r['event'] for r in in_gap))
        gap['pixel_duplicates'] = sum(r['event'] == 'DuplicateCheck' and r['a'] == 1 for r in in_gap)
        # A ring may not retain the entire gap's context, especially for earlier slow entries.
        gap['context_complete'] = all(
            any(r['thread'] == thread and r['start_us'] <= begin for r in recent) and
            any(r['thread'] == thread and r['start_us'] + r['duration_us'] >= end for r in recent)
            for thread in ['frontend', 'backend'])
    return dict(metadata=meta, retained_present_intervals=summaries, longest_present_gaps=gaps,
        retained_counts=dict(Counter(r['event'] for r in recent)),
        caveat='CPU submission trace; intervals are not measured display FPS. Stage durations overlap.')


def markdown(result, path):
    meta = result['metadata']
    out = ['# r9 debug 帧时间分析', '', f'输入：`{path.name}`；Magpie PID：{meta["pid"]}；'
        f'会话时长：{meta["duration_us"]/1_000_000:.3f} 秒。', '',
        '**这是 CPU 捕获／提交时间线，不是实际屏幕上屏测量。** 嵌套阶段的时间互相包含，不能相加。'
        '等待新捕获帧和低帧率视频可以产生正常的长间隔；不能把所有 ≥50 ms 事件自动判定为故障。', '',
        '## 数据保留范围', '', '| 线程 | 总事件数 | 被覆盖的近期事件 | 被覆盖的长事件 |',
        '| --- | ---: | ---: | ---: |']
    for name, lane in meta['lanes'].items():
        out.append(f'| {name} | {lane["total"]} | {lane["overwritten"]} | {lane["slow_overwritten"]} |')
    out += ['', '全会话计数和最大值保留；逐帧分布仅覆盖尚在近期环缓冲中的数据。不同线程保留起点可能不同。', '',
        '## 已保留的 Present 调用间隔', '',
        '仅统计同一交换链相邻 S_OK 调用的入口间隔，失败／遮挡会断开序列；不计第一帧之前的初始化。'
        'XeSSFG 代理交换链的调用频率也不代表其全部生成帧的上屏频率。', '',
        '| 交换链 | 间隔数 | 平均 ms | P99 ms | 最大 ms | 最慢 1% 平均 ms | 间隔换算 Low1 FPS |',
        '| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    for chain, s in result['retained_present_intervals'].items():
        if s:
            out.append(f'| {chain} | {s["samples"]} | {s["mean_ms"]:.3f} | {s["p99_ms"]:.3f} | '
                       f'{s["maximum_ms"]:.3f} | {s["slowest_1pct_mean_ms"]:.3f} | {s["interval_low1_fps"]:.2f} |')
    if not result['retained_present_intervals']:
        out.append('| 无可用样本 | — | — | — | — | — | — |')
    out += ['', '这里的 Low1 仅用于比较本诊断版的提交稳定性，不能直接替代 NVIDIA overlay 的 1% Low。', '',
        '## 全会话阶段统计', '', '| 线程／事件 | 次数 | 平均 ms | 最大 ms | ≥50 ms 次数 | 最大值起点 s |',
        '| --- | ---: | ---: | ---: | ---: | ---: |']
    for s in sorted(meta['statistics'], key=lambda s: s['maximum_us'], reverse=True):
        if s['maximum_us'] <= 0:
            continue
        out.append(f'| {s["thread"]}/{s["event"]} | {s["count"]} | '
            f'{s["total_us"]/s["count"]/1000:.3f} | {s["maximum_us"]/1000:.3f} | '
            f'{s["over50"]} | {s["maximum_start_us"]/1_000_000:.3f} |')
    out += ['', '## 最长提交间隔及上下文', '',
        '以下只列重叠阶段和事件计数，作为进一步调查的线索；没有事件不等于该阶段无问题，尤其在记录已覆盖时。']
    for g in result['longest_present_gaps']:
        out += ['', f'### {g["start_us"]/1_000_000:.3f} s：{g["duration_us"]/1000:.3f} ms，帧 {g["frame_id"]}', '',
            f'两线程近期上下文覆盖完整：{"是" if g["context_complete"] else "否"}。'
            f'区间内像素重复检测为真：{g["pixel_duplicates"]} 次。', '',
            '近期事件：' + ', '.join(f'{k}={v}' for k, v in sorted(g['recent_events'].items())) + '。']
        if g['overlapping_stages']:
            out.append('')
        for r in g['overlapping_stages']:
            out.append(f'- `{r["thread"]}/{r["event"]}`：{r["duration_us"]/1000:.3f} ms，'
                       f'起点 {r["start_us"]/1_000_000:.3f} s。')
    if not result['longest_present_gaps']:
        out += ['', '保留记录中没有 ≥50 ms 的 PresentGap；仍应查看全会话统计，确认是否存在被覆盖的长事件。']
    return '\n'.join(out) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    meta, rows = read_trace(args.trace)
    result = analyze(meta, rows)
    directory = args.output_dir or args.trace.parent
    directory.mkdir(parents=True, exist_ok=True)
    stem = directory / (args.trace.stem + '.analysis')
    md = stem.with_suffix('.analysis.md')
    js = stem.with_suffix('.analysis.json')
    md.write_text(markdown(result, args.trace), encoding='utf-8')
    js.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(md)
    print(js)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, IndexError) as error:
        print(f'Cannot analyze frame trace: {error}', file=sys.stderr)
        sys.exit(2)
