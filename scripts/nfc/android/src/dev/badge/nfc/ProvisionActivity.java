/* SPDX-License-Identifier: MIT */
package dev.badge.nfc;

import android.app.Activity;
import android.nfc.NfcAdapter;
import android.nfc.Tag;
import android.nfc.tech.NfcA;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Temporary P1 RF verifier. No BLE, storage, credential display or logging. */
public final class ProvisionActivity extends Activity implements NfcAdapter.ReaderCallback {
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private NfcAdapter adapter;
    private TextView status;
    private Button action;
    private String session;
    private int port;
    private final VerificationFlow flow = new VerificationFlow();
    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile boolean paused;
    private byte[] uid = new byte[0], pwd = new byte[0], pack = new byte[0];

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        session = getIntent().getStringExtra("session");
        port = getIntent().getIntExtra("port", 5052);
        getIntent().removeExtra("session");
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(32, 48, 32, 32);
        status = new TextView(this);
        status.setTextSize(20);
        action = new Button(this);
        action.setText("Authenticate tag");
        layout.addView(status);
        layout.addView(action);
        setContentView(layout);
        adapter = NfcAdapter.getDefaultAdapter(this);
        if (session == null || !session.matches("[0-9a-f]{64}") || adapter == null
                || !adapter.isEnabled() || port < 1 || port > 65535) {
            status.setText("Connect through USB or wireless debugging and start provisioning from the computer. NFC must be enabled.");
            action.setEnabled(false);
            return;
        }
        status.setText("Keep the DK powered and the phone connected through USB or wireless debugging. Start check 1 of 2.");
        action.setOnClickListener(view -> {
            action.setEnabled(false);
            if (flow.start()) {
                status.setText("Fetching the one-use challenge for check 1 of 2...");
                worker.execute(() -> {
                    try {
                        JSONObject challenge = request("challenge", null);
                        if (challenge.getInt("proof_version") != NfcProof.VERSION) { throw new IllegalStateException(); }
                        uid = decode(challenge.getString("uid"), 7);
                        pwd = decode(challenge.getString("pwd"), 4);
                        pack = decode(challenge.getString("pack"), 2);
                        if (!flow.challengeReady()) { clearCredentials(); return; }
                        arm("Check 1 of 2 ready: tap the tag and hold still for the generated-password check.");
                    } catch (Exception ignored) { fail("Could not fetch the computer challenge."); }
                });
            } else if (flow.startSecond()) {
                arm("Check 2 of 2 ready: tap again and hold still to confirm the generated password in a fresh session.");
            } else {
                fail("The verification session is no longer active.");
            }
        });
    }

    private void arm(String message) {
        runOnUiThread(() -> {
            if (paused || flow.state() == VerificationFlow.State.FAILED) { return; }
            status.setText(message);
            try {
                adapter.enableReaderMode(this, this,
                        NfcAdapter.FLAG_READER_NFC_A | NfcAdapter.FLAG_READER_SKIP_NDEF_CHECK, null);
            } catch (RuntimeException ignored) { fail("Could not enable NFC reader mode."); }
        });
    }

    @Override public void onTagDiscovered(Tag tag) {
        if (paused) { return; }
        final int step = flow.takeTap();
        if (step == 0) { return; } // Exactly one discovery may claim each check.
        runOnUiThread(() -> {
            if (!paused && flow.state() != VerificationFlow.State.FAILED) {
                status.setText("Check " + step + " of 2: tag detected, checking RF authentication...");
            }
        });
        worker.execute(() -> {
            NfcA radio = NfcA.get(tag);
            try {
                if (radio == null || !Arrays.equals(uid, tag.getId())) { throw new IllegalStateException(); }
                radio.connect();
                if (!NfcProof.supportedTag(radio::transceive)) { throw new IllegalStateException(); }
                radio.setTimeout(2000);
                if (!NfcProof.verifiedSession(radio::transceive, pwd, pack)) { throw new IllegalStateException(); }
                if (step == 1) {
                    // Close communication before exposing the next button.
                    radio.close();
                    if (!flow.firstPassed()) { return; }
                    runOnUiThread(() -> {
                        if (paused || flow.state() != VerificationFlow.State.WAIT_REMOVAL) { return; }
                        // Keep reader mode active: disabling it here can allow
                        // ordinary NDEF dispatch to open a browser and pause us.
                        status.setText("First check passed. Move the phone well away and wait for removal detection.");
                        action.setText("Waiting for tag removal...");
                        action.setEnabled(false);
                        try {
                            boolean watching = adapter.ignore(tag, 750, () -> {
                                if (!paused && flow.removed()) {
                                    status.setText("Tag removed. Press Start check 2, then tap again.");
                                    action.setText("Start check 2 of 2");
                                    action.setEnabled(true);
                                }
                            }, ui);
                            if (!watching) { fail("Could not monitor tag removal; restart and hold still during check 1."); }
                        } catch (RuntimeException ignored) { fail("Could not monitor tag removal."); }
                    });
                } else {
                    clearCredentials();
                    if (paused || flow.state() != VerificationFlow.State.SECOND_CHECKING) { return; }
                    JSONObject result = new JSONObject();
                    result.put("uid", hex(uid));
                    result.put("generated_ok", true);
                    result.put("repeated_generated_ok", true);
                    result.put("fresh_sessions", true);
                    result.put("proof_version", NfcProof.VERSION);
                    if (!request("proof", result).getBoolean("accepted")) { throw new IllegalStateException(); }
                    if (!flow.secondPassed()) { return; }
                    runOnUiThread(() -> {
                        // Keep NDEF dispatch suppressed while the tag is still
                        // in range. Reader mode ends when the activity pauses.
                        status.setText("Both RF checks passed. Remove the phone from the tag and finish on the computer.");
                        action.setEnabled(false);
                    });
                }
            } catch (Exception ignored) { fail("Check " + step + " of 2 failed (RF authentication or computer connection)."); }
            finally {
                if (radio != null) { try { radio.close(); } catch (Exception ignored) {} }
            }
        });
    }

    private JSONObject request(String path, JSONObject body) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) new URL("http://127.0.0.1:" + port + "/" + path).openConnection();
        connection.setConnectTimeout(5000);
        connection.setReadTimeout(5000);
        connection.setRequestProperty("Authorization", "Bearer " + session);
        connection.setUseCaches(false);
        try {
            if (body != null) {
                connection.setRequestMethod("POST");
                connection.setDoOutput(true);
                byte[] data = body.toString().getBytes("UTF-8");
                connection.setFixedLengthStreamingMode(data.length);
                connection.getOutputStream().write(data);
            }
            if (connection.getResponseCode() != 200) { throw new IllegalStateException(); }
            try (InputStream input = connection.getInputStream(); ByteArrayOutputStream buffer = new ByteArrayOutputStream()) {
                byte[] block = new byte[256];
                int count;
                while ((count = input.read(block)) >= 0) {
                    if (buffer.size() + count > 1024) { throw new IllegalStateException(); }
                    buffer.write(block, 0, count);
                }
                return new JSONObject(buffer.toString("UTF-8"));
            }
        } finally { connection.disconnect(); }
    }

    private static byte[] decode(String value, int length) {
        if (value.length() != length * 2 || !value.matches("[0-9a-f]+")) { throw new IllegalArgumentException(); }
        byte[] result = new byte[length];
        for (int i = 0; i < length; ++i) { result[i] = (byte) Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16); }
        return result;
    }

    private static String hex(byte[] value) {
        StringBuilder result = new StringBuilder();
        for (byte part : value) { result.append(String.format("%02x", part & 255)); }
        return result.toString();
    }

    private void clearCredentials() {
        Arrays.fill(pwd, (byte) 0);
        Arrays.fill(pack, (byte) 0);
    }

    private void fail(String reason) {
        final boolean firstFailure = flow.abort();
        clearCredentials();
        if (!firstFailure) { return; }
        runOnUiThread(() -> {
            // Keep reader mode until onPause: restoring ordinary NFC polling
            // here can open the tag URL and hide the actual check failure.
            status.setText(reason + " Verification stopped; restart provisioning from the computer.");
            action.setEnabled(false);
        });
    }

    @Override public void onResume() { super.onResume(); paused = false; }
    @Override public void onPause() {
        paused = true;
        if (flow.state() != VerificationFlow.State.COMPLETE) {
            fail("The helper lost focus or the phone locked.");
        }
        if (adapter != null) {
            try { adapter.disableReaderMode(this); } catch (RuntimeException ignored) {}
        }
        clearCredentials();
        super.onPause();
    }
    @Override public void onDestroy() { worker.shutdownNow(); session = null; super.onDestroy(); }
}
