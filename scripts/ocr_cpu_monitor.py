"""Lightweight Windows system/process CPU sampling, no external dependency."""
import ctypes as C
from ctypes import wintypes as W
import os
import threading
import time

class CpuMonitor:
    def __init__(self,interval=.5):
        self.interval=interval;self.phase='model_load';self.samples=[];self.error=None
        self.event=threading.Event();self.thread=None;self.previous=None
        self.lock=threading.Lock();self.started=time.perf_counter()
    def _read(self):
        if os.name!='nt':raise RuntimeError('GetSystemTimes CPU monitor requires Windows')
        d=C.WinDLL('kernel32',use_last_error=True)
        d.GetSystemTimes.argtypes=[C.POINTER(W.FILETIME)]*3;d.GetSystemTimes.restype=W.BOOL
        d.GetCurrentProcess.restype=W.HANDLE
        d.GetProcessTimes.argtypes=[W.HANDLE]+[C.POINTER(W.FILETIME)]*4;d.GetProcessTimes.restype=W.BOOL
        idle,kernel,user=W.FILETIME(),W.FILETIME(),W.FILETIME()
        if not d.GetSystemTimes(C.byref(idle),C.byref(kernel),C.byref(user)):raise C.WinError(C.get_last_error())
        created,exited,pk,pu=W.FILETIME(),W.FILETIME(),W.FILETIME(),W.FILETIME()
        if not d.GetProcessTimes(d.GetCurrentProcess(),C.byref(created),C.byref(exited),C.byref(pk),C.byref(pu)):raise C.WinError(C.get_last_error())
        def tick(f):return f.dwLowDateTime+(f.dwHighDateTime<<32)
        return time.perf_counter(),tick(idle),tick(kernel)+tick(user),tick(pk)+tick(pu)
    def sample(self):
        with self.lock:
            try:
                now=self._read()
                if self.previous:
                    old=self.previous;total=now[2]-old[2];elapsed=now[0]-old[0]
                    if total>0 and elapsed>0:
                        self.samples.append({'elapsed_s':now[0]-self.started,'phase':self.phase,
                            'system_percent':max(0,min(100,100*(1-(now[1]-old[1])/total))),
                            'process_percent_normalized':100*(now[3]-old[3])/1e7/elapsed/(os.cpu_count() or 1)})
                self.previous=now
            except Exception as exc:self.error=str(exc)
    def start(self):
        self.sample()
        def loop():
            while not self.event.wait(self.interval):self.sample()
        self.thread=threading.Thread(target=loop,daemon=True);self.thread.start()
    def stop(self):
        self.event.set()
        if self.thread:self.thread.join(timeout=2)
        self.sample()
    def report(self):
        selected=[s for s in self.samples if s['phase']=='measurement']
        return {'source':'Windows GetSystemTimes/GetProcessTimes','interval_s':self.interval,
                'logical_cpus':os.cpu_count(),'error':self.error,'samples':self.samples,
                'measurement_system_mean_percent':sum(s['system_percent'] for s in selected)/len(selected) if selected else None,
                'measurement_system_max_percent':max((s['system_percent'] for s in selected),default=None)}
