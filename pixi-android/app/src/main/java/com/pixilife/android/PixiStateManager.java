package com.pixilife.android;

public final class PixiStateManager {
    public enum State {IDLE,LISTENING,THINKING,SPEAKING,VISION,FETCHING_WEATHER,FETCHING_LOCATION,SEARCHING_NEARBY,CALENDAR,NOTIFICATION,REMINDER,ALARM,ERROR,SLEEPING,SENSOR_ALERT,PLAYFUL,MOCK_ANGRY}
    public interface Listener{void onStateChanged(State previous,State current);}
    private volatile State state=State.IDLE; private Listener listener;
    public synchronized void setState(State next){if(next==null||next==state)return;State old=state;state=next;if(listener!=null)listener.onStateChanged(old,next);}
    public State getState(){return state;} public void setListener(Listener value){listener=value;}
}
