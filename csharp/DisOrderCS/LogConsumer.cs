using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace uk.org.greenend.DisOrder
{
  public class LogConsumer
  {
    public LogConsumer()
    {
      OnAdopted = (_1, _2, _3) => { };
      OnCompleted = (_1, _2) => { };
      OnFailed = (_1, _2, _3) => { };
      OnMoved = (_1, _2) => { };
      OnPlaying = (_1, _2, _3) => { };
      OnQueue = (_1, _2) => { };
      OnRecentAdded = (_1, _2) => { };
      OnRecentRemoved = (_1, _2) => { };
      OnRemoved = (_1, _2, _3) => { };
      OnRescanned = (_1) => { };
      OnScratched = (_1, _2, _3) => { };
      OnState = (_1, _2) => { };
      OnUserAdd = (_1, _2) => { };
      OnUserDelete = (_1, _2) => { };
      OnUserEdit = (_1, _2, _3) => { };
      OnUserConfirm = (_1, _2) => { };
      OnVolume = (_1, _2, _3) => { };
    }

    public Action<DateTime, string, string> OnAdopted { get; set; }
    public virtual void Adopted(DateTime when, string id, string username) { OnAdopted(when, id, username); }
    
    public Action<DateTime, string> OnCompleted { get; set; }
    public virtual void Completed(DateTime when, string track) { OnCompleted(when, track);}

    public Action<DateTime, string, string> OnFailed { get; set; }
    public virtual void Failed(DateTime when, string track, string error) { OnFailed(when, track, error); }

    public Action<DateTime, string> OnMoved { get; set; }
    public virtual void Moved(DateTime when, string username) { OnMoved(when, username); }

    public Action<DateTime, string, string> OnPlaying { get; set; }
    public virtual void Playing(DateTime when, string track, string username) { OnPlaying(when, track, username); }

    public Action<DateTime, QueueEntry> OnQueue { get; set; }
    public virtual void Queue(DateTime when, QueueEntry qe) { OnQueue(when, qe); }

    public Action<DateTime, QueueEntry> OnRecentAdded { get; set; }
    public virtual void RecentAdded(DateTime when, QueueEntry qe) { OnRecentAdded(when, qe);  }

    public Action<DateTime, string> OnRecentRemoved { get; set; }
    public virtual void RecentRemoved(DateTime when, string id) { OnRecentRemoved(when, id);  }

    public Action<DateTime, string, string> OnRemoved { get; set; }
    public virtual void Removed(DateTime when, string id, string username) { OnRemoved(when, id, username); }

    public Action<DateTime> OnRescanned { get; set; }
    public virtual void Rescanned(DateTime when) { OnRescanned(when); }

    public Action<DateTime, string, string> OnScratched { get; set; }
    public virtual void Scratched(DateTime when, string track, string username) { OnScratched(when, track, username); }

    public Action<DateTime, string> OnState { get; set; }
    public virtual void State(DateTime when, string keyword) { OnState(when, keyword); }

    public Action<DateTime, string> OnUserAdd { get; set; }
    public virtual void UserAdd(DateTime when, string username) { OnUserAdd(when, username);  }

    public Action<DateTime, string> OnUserDelete { get; set; }
    public virtual void UserDelete(DateTime when, string username) { OnUserDelete(when, username); }

    public Action<DateTime, string, string> OnUserEdit { get; set; }
    public virtual void UserEdit(DateTime when, string username, string property) { OnUserEdit(when, username, property); }

    public Action<DateTime, string> OnUserConfirm { get; set; }
    public virtual void UserConfirm(DateTime when, string username) { OnUserConfirm(when, username); }

    public Action<DateTime, int, int> OnVolume { get; set; }
    public virtual void Volume(DateTime when, int left, int right) { OnVolume(when, left, right); }
  }
}
