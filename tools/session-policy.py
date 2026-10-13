"""Startup order independent of Windows and of native-loader success."""
class SessionLifetime:
    """A transient bootstrap PID must not end the launcher's startup window."""
    def __init__(self, started, startup_seconds=180, restart_seconds=60):
        self.deadline=started+startup_seconds
        self.restart_seconds=restart_seconds
        self.last_seen=started
        self.ready=False

    def expired(self, now, candidates, loaded):
        if candidates:self.last_seen=now
        if loaded:self.ready=True
        if candidates:return False
        if not self.ready:return now>=self.deadline
        return now-self.last_seen>=self.restart_seconds

def process_state(state,activate,queue_native,record):
    players=state.get('players',[])
    if players and players[0].get('0x18c')==3 and players[0].get('0xd8')==0xffffffff:
        activate(state['pid'])  # Full executable/layout/host guards live in activate().
        record(status='human-slot-assigned',pid=state['pid'],slot=1)
    # Never wait for DLL loading before assigning the imported human slot.
    queue_native(state)
