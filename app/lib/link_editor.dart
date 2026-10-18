// SPDX-License-Identifier: MIT
import 'dart:async';
import 'package:flutter/material.dart';
import 'protocol.dart';
import 'qr_capacity.dart';
import 'studio_model.dart';

/// Own the controllers until the dialog has actually unmounted, including its
/// reverse animation. Disposing them when showDialog completes is too early.
class LinkEditorDialog extends StatefulWidget {
  const LinkEditorDialog({super.key, this.original});
  final SavedLink? original;
  @override
  State<LinkEditorDialog> createState() => _LinkEditorDialogState();
}

class _LinkEditorDialogState extends State<LinkEditorDialog> {
  final form = GlobalKey<FormState>();
  Timer? errorTimer;
  String? urlError;
  void showUrlError(String? error) {
    errorTimer?.cancel();
    setState(() => urlError = error);
    if (error != null) {
      errorTimer = Timer(const Duration(seconds: 1), () {
        if (mounted) setState(() => urlError = null);
      });
    }
  }

  String? validateUrl() {
    try {
      validateValue(BadgeField.url, encodeAscii(normalizeUrl(url.text.trim())));
      return null;
    } on FormatException catch (error) {
      return error.message;
    }
  }

  late final label = TextEditingController(text: widget.original?.label ?? '');
  late final url = TextEditingController(
    text: widget.original?.url ?? 'https://',
  );
  @override
  void dispose() {
    errorTimer?.cancel();
    label.dispose();
    url.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final value = normalizeUrl(url.text.trim());
    final fit = QrCapacity.fit(value);
    final maximum = QrCapacity.isAlphanumeric(value) ? 25 : 17;
    return AlertDialog(
      title: Text(widget.original == null ? 'Add a custom link' : 'Edit link'),
      content: SizedBox(
        width: 420,
        child: SingleChildScrollView(
          child: Form(
            key: form,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                TextFormField(
                  controller: label,
                  maxLength: 32,
                  decoration: const InputDecoration(
                    labelText: 'Label',
                    hintText: 'My portfolio',
                  ),
                  validator: (v) => v == null || v.trim().isEmpty
                      ? 'Give this link a label.'
                      : null,
                ),
                const SizedBox(height: 12),
                TextFormField(
                  key: const Key('link-url'),
                  controller: url,
                  keyboardType: TextInputType.url,
                  autocorrect: false,
                  enableSuggestions: false,
                  onChanged: (_) {
                    final value = normalizeUrl(url.text.trim());
                    final limit = QrCapacity.isAlphanumeric(value) ? 25 : 17;
                    showUrlError(
                      value.length > limit
                          ? 'Maximum $limit characters including HTTP(S)://.'
                          : null,
                    );
                  },
                  decoration: InputDecoration(
                    labelText: 'URL',
                    errorText: urlError,
                    errorMaxLines: 3,
                    hintText: 'https://EXAMPLE.COM',
                    helperText:
                        '${value.length}/$maximum total characters${fit == null ? '' : ' / correction ${fit.correction}'}',
                    helperMaxLines: 2,
                  ),
                ),
                const SizedBox(height: 12),
                Text(
                  'Up to 25 uppercase QR characters or 17 ASCII bytes, including HTTP(S)://.',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                const ExpansionTile(
                  tilePadding: EdgeInsets.zero,
                  title: Text('QR limits'),
                  dense: true,
                  children: [
                    Text(
                      'Uppercase letters, digits and QR punctuation fit more. Path case is preserved. 21 x 21 QR with a full quiet zone. Correction H, Q, M or L is selected automatically; longer links have less error recovery.',
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        FilledButton(
          onPressed: () {
            final labelValid = form.currentState!.validate();
            final error = validateUrl();
            showUrlError(error);
            if (labelValid && error == null) {
              Navigator.pop(
                context,
                SavedLink(label.text.trim(), normalizeUrl(url.text.trim())),
              );
            }
          },
          child: const Text('Save link'),
        ),
      ],
    );
  }
}

class LibraryNameDialog extends StatefulWidget {
  const LibraryNameDialog({
    super.key,
    required this.initial,
    this.renaming = false,
  });
  final String initial;
  final bool renaming;
  @override
  State<LibraryNameDialog> createState() => _LibraryNameDialogState();
}

class _LibraryNameDialogState extends State<LibraryNameDialog> {
  late final name = TextEditingController(text: widget.initial);
  @override
  void dispose() {
    name.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => AlertDialog(
    title: Text(widget.renaming ? 'Rename' : 'Save to library'),
    content: TextField(
      controller: name,
      autofocus: true,
      maxLength: 40,
      decoration: const InputDecoration(labelText: 'Name'),
      onChanged: (_) => setState(() {}),
    ),
    actions: [
      TextButton(
        onPressed: () => Navigator.pop(context),
        child: const Text('Cancel'),
      ),
      FilledButton(
        onPressed: name.text.trim().isEmpty
            ? null
            : () => Navigator.pop(context, name.text.trim()),
        child: Text(widget.renaming ? 'Rename' : 'Save'),
      ),
    ],
  );
}
