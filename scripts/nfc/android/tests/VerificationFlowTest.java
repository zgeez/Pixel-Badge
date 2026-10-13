/* SPDX-License-Identifier: MIT */
import dev.badge.nfc.VerificationFlow;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicInteger;

public final class VerificationFlowTest {
    private static void check(boolean condition) { if (!condition) { throw new AssertionError(); } }
    private static VerificationFlow first() {
        VerificationFlow flow = new VerificationFlow();
        check(flow.start()); check(flow.challengeReady()); check(flow.takeTap() == 1);
        return flow;
    }
    public static void main(String[] args) throws Exception {
        VerificationFlow full = first();
        check(!full.startSecond()); check(full.takeTap() == 0);
        check(full.firstPassed()); check(!full.startSecond());
        check(full.takeTap() == 0); // Repeated discoveries are not a fresh check.
        check(full.removed()); check(full.startSecond());
        check(full.takeTap() == 2); check(full.takeTap() == 0);
        check(full.secondPassed()); check(!full.abort());
        check(full.state() == VerificationFlow.State.COMPLETE);

        VerificationFlow paused = first();
        check(paused.firstPassed()); paused.abort();
        check(!paused.removed()); check(!paused.startSecond());
        check(paused.takeTap() == 0); check(paused.state() == VerificationFlow.State.FAILED);
        check(!paused.abort()); // A later focus loss preserves the first failure.

        VerificationFlow lateFirst = first(); lateFirst.abort();
        check(!lateFirst.firstPassed()); check(!lateFirst.removed());
        VerificationFlow lateChallenge = new VerificationFlow();
        check(lateChallenge.start()); lateChallenge.abort(); check(!lateChallenge.challengeReady());

        VerificationFlow lateSecond = first();
        check(lateSecond.firstPassed()); check(lateSecond.removed()); check(lateSecond.startSecond());
        check(lateSecond.takeTap() == 2); lateSecond.abort(); check(!lateSecond.secondPassed());

        VerificationFlow premature = new VerificationFlow();
        check(premature.takeTap() == 0); check(!premature.firstPassed());
        check(!premature.removed()); check(!premature.secondPassed());

        VerificationFlow race = new VerificationFlow();
        check(race.start()); check(race.challengeReady());
        CountDownLatch start = new CountDownLatch(1);
        AtomicInteger claimed = new AtomicInteger();
        Runnable discovery = () -> {
            try { start.await(); } catch (InterruptedException e) { throw new AssertionError(e); }
            if (race.takeTap() == 1) { claimed.incrementAndGet(); }
        };
        Thread one = new Thread(discovery), two = new Thread(discovery);
        one.start(); two.start(); start.countDown(); one.join(); two.join();
        check(claimed.get() == 1);
        System.out.println("NFC verification flow: 7 scenarios passed");
    }
}
