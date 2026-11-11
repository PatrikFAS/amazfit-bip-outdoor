#!/usr/bin/env python3
"""Convert a user-supplied tide prediction CSV to BIPT v1. No network access."""
import argparse, csv, datetime as dt, math, pathlib, struct, zlib
MAX_EVENTS=64

def bounded_text(s, limit, field, required=True):
    b=s.encode('ascii')
    if (required and not b) or len(b)>limit or any(x<32 or x>126 for x in b):
        raise ValueError(f'{field} must be printable ASCII, 1..{limit} bytes')
    return b

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('csv');p.add_argument('output')
    p.add_argument('--station',required=True);p.add_argument('--latitude',required=True,type=float);p.add_argument('--longitude',required=True,type=float)
    p.add_argument('--utc-offset-minutes',required=True,type=int);p.add_argument('--source',required=True);p.add_argument('--note',default='')
    p.add_argument('--generated-utc',required=True,help='Unix UTC seconds');p.add_argument('--expires-utc',required=True,help='Unix UTC seconds')
    a=p.parse_args()
    if not math.isfinite(a.latitude) or not -90<=a.latitude<=90 or not math.isfinite(a.longitude) or not -180<=a.longitude<=180: raise ValueError('invalid station coordinates')
    if not -840<=a.utc_offset_minutes<=840: raise ValueError('UTC offset outside -14:00..+14:00')
    generated=int(a.generated_utc);expires=int(a.expires_utc)
    if generated<0 or expires<=generated or expires>0xffffffff: raise ValueError('invalid generated/expiry UTC seconds')
    station=bounded_text(a.station,32,'station');source=bounded_text(a.source,32,'source');note=bounded_text(a.note,48,'note',False)
    rows=[]
    with open(a.csv,newline='',encoding='utf-8') as f:
      reader=csv.DictReader(f)
      if reader.fieldnames is None or not {'timestamp_utc','kind','height_m'}<=set(reader.fieldnames): raise ValueError('CSV needs timestamp_utc,kind,height_m columns')
      for row in reader:
        t=dt.datetime.fromisoformat(row['timestamp_utc'].replace('Z','+00:00'))
        if t.tzinfo is None or t.utcoffset()!=dt.timedelta(0): raise ValueError('timestamp_utc values must include UTC Z or +00:00')
        epoch=int(t.timestamp());kind=row['kind'].strip().lower()
        if epoch < 0 or epoch > 0xffffffff: raise ValueError('event timestamp outside unsigned UTC seconds range')
        if kind not in ('high','low'): raise ValueError('kind must be high or low')
        height=float(row['height_m'])
        if not math.isfinite(height) or not -327.68<=height<=327.67: raise ValueError('height_m outside signed centimetre range')
        cm=round(height*100)
        if epoch>expires: raise ValueError('event occurs after expiry UTC')
        rows.append((epoch,cm,1 if kind=='high' else 2))
    if len(rows)>MAX_EVENTS: raise ValueError('at most 64 events are allowed')
    if any(rows[k][0]>=rows[k+1][0] for k in range(len(rows)-1)): raise ValueError('events must be strictly sorted by timestamp_utc')
    b=bytearray(144+8*len(rows)+4);b[:5]=b'BIPT\x01';b[5]=len(rows)
    struct.pack_into('<IIiih',b,8,generated,expires,round(a.latitude*1e7),round(a.longitude*1e7),a.utc_offset_minutes)
    b[26:29]=bytes((len(station),len(source),len(note)))
    b[32:32+len(station)]=station;b[64:64+len(source)]=source;b[96:96+len(note)]=note
    for k,(epoch,cm,kind) in enumerate(rows):struct.pack_into('<IhBB',b,144+8*k,epoch,cm,kind,0)
    struct.pack_into('<I',b,len(b)-4,zlib.crc32(b[:-4])&0xffffffff)
    pathlib.Path(a.output).write_bytes(b)
if __name__=='__main__':
 try:main()
 except (ValueError,OverflowError,UnicodeError) as e:raise SystemExit(f'import-tides: {e}')
