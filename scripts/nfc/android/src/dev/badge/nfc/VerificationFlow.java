/* SPDX-License-Identifier: MIT */
package dev.badge.nfc;

/** Thread-safe RF workflow; no credentials or Android dependencies. */
public final class VerificationFlow {
    public enum State {
        START, CHALLENGE, FIRST_ARMED, FIRST_CHECKING, WAIT_REMOVAL,
        SECOND_READY, SECOND_ARMED, SECOND_CHECKING, COMPLETE, FAILED
    }
    private State state = State.START;
    public synchronized State state() { return state; }
    private boolean move(State from, State to) {
        if (state != from) { return false; }
        state = to;
        return true;
    }
    public synchronized boolean start() { return move(State.START, State.CHALLENGE); }
    public synchronized boolean challengeReady() { return move(State.CHALLENGE, State.FIRST_ARMED); }
    public synchronized int takeTap() {
        if (move(State.FIRST_ARMED, State.FIRST_CHECKING)) { return 1; }
        if (move(State.SECOND_ARMED, State.SECOND_CHECKING)) { return 2; }
        return 0;
    }
    public synchronized boolean firstPassed() { return move(State.FIRST_CHECKING, State.WAIT_REMOVAL); }
    public synchronized boolean removed() { return move(State.WAIT_REMOVAL, State.SECOND_READY); }
    public synchronized boolean startSecond() { return move(State.SECOND_READY, State.SECOND_ARMED); }
    public synchronized boolean secondPassed() { return move(State.SECOND_CHECKING, State.COMPLETE); }
    /** Only the first failure owns the diagnostic; later lifecycle callbacks
     * cannot replace it with a generic focus-loss message. */
    public synchronized boolean abort() {
        if (state == State.COMPLETE || state == State.FAILED) { return false; }
        state = State.FAILED;
        return true;
    }
}
