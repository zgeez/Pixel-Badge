// SPDX-License-Identifier: MIT
part of 'studio_home.dart';

extension _CompactWorkspace on _BadgeHomeState {
  Widget compactBuild(BuildContext context) {
    final wide = MediaQuery.sizeOf(context).width >= 850;
    const title = 'Pixel Badge';
    return Scaffold(
      appBar: AppBar(
        toolbarHeight: 48,
        title: Text(title, style: Theme.of(context).textTheme.titleMedium),
        actions: [
          if (page == 1)
            IconButton(
              tooltip: 'Links library',
              icon: const Icon(Icons.bookmarks_outlined),
              onPressed: loadingDraft ? null : openLinksLibrary,
            ),
          if (page == 2 || page == 3)
            IconButton(
              tooltip: 'Open library',
              icon: const Icon(Icons.bookmarks_outlined),
              onPressed: loadingDraft
                  ? null
                  : () => openDesignLibrary(page == 2),
            ),
          IconButton(
            tooltip: badge.connected ? 'Refresh settings' : 'Connect',
            onPressed: connecting || scanning || badge.busy
                ? null
                : () => unawaited(
                    badge.connected ? action(badge.refresh) : findBadge(),
                  ),
            icon: connecting || scanning
                ? const SizedBox(
                    width: 18,
                    height: 18,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : Icon(
                    badge.connected
                        ? Icons.bluetooth_connected
                        : Icons.bluetooth,
                  ),
          ),
          PopupMenuButton<String>(
            tooltip: 'Badge options',
            onSelected: (v) {
              if (v == 'disconnect') unawaited(action(badge.disconnect));
              if (v == 'pairing') {
                showDialog<void>(
                  context: context,
                  builder: (c) => AlertDialog(
                    title: const Text('Pairing'),
                    content: const Text(
                      'A paired phone can reconnect directly. For a new phone, hold Action on the badge for one second, release, then connect and enter its code.',
                    ),
                    actions: [
                      TextButton(
                        onPressed: () => Navigator.pop(c),
                        child: const Text('Done'),
                      ),
                    ],
                  ),
                );
              }
              if (v == 'about') {
                showAboutDialog(
                  context: context,
                  applicationName: 'Pixel Badge',
                  applicationVersion: '0.10.3',
                  children: [const Text('32 x 32 light. Made yours.')],
                );
              }
            },
            itemBuilder: (_) => [
              if (badge.connected)
                const PopupMenuItem(
                  value: 'disconnect',
                  child: MenuActionLabel(
                    Icons.bluetooth_disabled,
                    'Disconnect',
                  ),
                ),
              const PopupMenuItem(
                value: 'pairing',
                child: MenuActionLabel(Icons.help_outline, 'Pairing help'),
              ),
              const PopupMenuItem(
                value: 'about',
                child: MenuActionLabel(Icons.info_outline, 'About'),
              ),
            ],
          ),
        ],
      ),
      body: SafeArea(
        child: Row(
          children: [
            if (wide)
              NavigationRail(
                selectedIndex: page,
                labelType: NavigationRailLabelType.all,
                onDestinationSelected: (v) => update(() => page = v),
                destinations: [
                  for (var i = 0; i < 5; i++)
                    NavigationRailDestination(
                      icon: Icon(pageIcons[i]),
                      label: Text(pageLabels[i]),
                    ),
                ],
              ),
            Expanded(
              child: Center(
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 760),
                  child: Padding(
                    padding: const EdgeInsets.fromLTRB(12, 0, 12, 8),
                    child: Column(
                      children: [
                        if (connecting)
                          Padding(
                            padding: const EdgeInsets.symmetric(vertical: 6),
                            child: Column(
                              children: [
                                Text(
                                  badge.connected
                                      ? 'Loading settings'
                                      : connectionBond == true
                                      ? 'Reconnecting'
                                      : 'Connecting',
                                ),
                                Text(
                                  badge.connected
                                      ? 'Reading badge... ${(badge.uploadProgress * 100).floor()}%'
                                      : connectionBond == false
                                      ? 'Enter the code on your badge in the phone pairing dialog.'
                                      : 'Establishing a secure connection...',
                                  textAlign: TextAlign.center,
                                  style: Theme.of(context).textTheme.bodySmall,
                                ),
                              ],
                            ),
                          ),
                        if (badge.busy || importing || loadingDraft)
                          SizedBox(
                            height: 2,
                            child: LinearProgressIndicator(
                              value: badge.busy && badge.uploadProgress < 1
                                  ? badge.uploadProgress
                                  : null,
                            ),
                          ),
                        if (draftMessage?.startsWith('Draft not saved') ==
                                true ||
                            draftMessage?.startsWith('Your saved draft') ==
                                true)
                          Row(
                            children: [
                              const Icon(Icons.error_outline, size: 18),
                              const SizedBox(width: 6),
                              const Expanded(
                                child: Text(
                                  'Changes not saved',
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ),
                              TextButton(
                                onPressed: () => unawaited(saveDraft()),
                                child: const Text('Retry'),
                              ),
                            ],
                          ),
                        Expanded(
                          child: AbsorbPointer(
                            absorbing: loadingDraft || (importing && page == 3),
                            child: [
                              compactDashboard,
                              compactLinks,
                              compactText,
                              compactStudio,
                              () => compactAudio(context),
                            ][page](),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
      bottomNavigationBar: wide
          ? null
          : NavigationBar(
              height: 64,
              labelBehavior: NavigationDestinationLabelBehavior.alwaysShow,
              selectedIndex: page,
              onDestinationSelected: (v) => update(() => page = v),
              destinations: [
                for (var i = 0; i < 5; i++)
                  NavigationDestination(
                    icon: Icon(pageIcons[i]),
                    label: pageLabels[i],
                  ),
              ],
            ),
    );
  }

  Widget canvasSpace(Widget child) => LayoutBuilder(
    builder: (context, box) => Center(
      child: SizedBox.square(
        dimension: box.maxHeight < box.maxWidth ? box.maxHeight : box.maxWidth,
        child: child,
      ),
    ),
  );
  Widget compactDashboard() {
    if (!badge.connected) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.blur_on,
              size: 72,
              color: Theme.of(context).colorScheme.primary,
            ),
            const SizedBox(height: 16),
            Text(
              'Ready when you are',
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.titleLarge,
            ),
            const SizedBox(height: 16),
            FilledButton.icon(
              onPressed: connecting || scanning
                  ? null
                  : () => unawaited(findBadge()),
              icon: const Icon(Icons.bluetooth),
              label: const Text('Find badge'),
            ),
          ],
        ),
      );
    }
    final mode = badge.mode.clamp(0, modeLabels.length - 1);
    Widget picture;
    if (mode == 1 && badge.values[BadgeField.text] != null) {
      final design = TextDesign.decode(badge.values[BadgeField.text]!);
      picture = preview(
        (t) => design.render(t, fallback: badge.name),
        animate: true,
      );
    } else if ((mode == 2 || mode == 3) &&
        badge.values[BadgeField.artwork] != null) {
      final design = appliedArtwork!;
      picture = preview(
        (t) => design.frames[mode == 3 ? design.frameAt(t) : 0],
        animate: mode == 3,
      );
    } else {
      picture = DecoratedBox(
        decoration: BoxDecoration(
          color: Theme.of(context).colorScheme.surfaceContainer,
          borderRadius: BorderRadius.circular(24),
        ),
        child: Center(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                mode == 0
                    ? Icons.qr_code_2
                    : mode == 6
                    ? Icons.water
                    : Icons.graphic_eq,
                size: 92,
              ),
              if (mode == 0)
                Padding(
                  padding: const EdgeInsets.all(12),
                  child: Text(
                    badge.url,
                    textAlign: TextAlign.center,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
            ],
          ),
        ),
      );
    }
    return Column(
      children: [
        Row(
          children: [
            Expanded(
              child: SelectionControl<int>(
                key: const Key('display-mode'),
                title: 'Display mode',
                value: mode,
                expand: true,
                choices: [
                  for (var i = 0; i < badge.capabilities.modeCount; i++)
                    BadgeChoice(
                      i,
                      modeLabels[i],
                      icon: [
                        Icons.qr_code_2,
                        Icons.text_fields,
                        Icons.draw_outlined,
                        Icons.animation,
                        Icons.graphic_eq,
                        Icons.equalizer,
                        Icons.water,
                      ][i],
                    ),
                ],
                onChanged: ready
                    ? (v) => unawaited(
                        action(() => badge.apply(BadgeField.mode, [v])),
                      )
                    : null,
              ),
            ),
            if (badge.capabilities.hasMotion)
              IconButton(
                tooltip: 'Motion settings',
                icon: const Icon(Icons.screen_rotation_outlined),
                onPressed: openMotion,
              ),
          ],
        ),
        Expanded(
          child: Padding(
            padding: const EdgeInsets.symmetric(vertical: 12),
            child: canvasSpace(picture),
          ),
        ),
        Row(
          children: [
            const Icon(Icons.brightness_6_outlined, size: 20),
            const SizedBox(width: 8),
            for (var i = 0; i < badge.brightnessPresets.length; i++)
              Expanded(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 2),
                  child: ChoiceChip(
                    showCheckmark: false,
                    label: Text(
                      [
                        badge.capabilities.hasEditorV2 ? '5%' : '6%',
                        '25%',
                        '50%',
                        '100%',
                      ][i],
                    ),
                    selected: badge.brightness == badge.brightnessPresets[i],
                    onSelected: ready
                        ? (_) => unawaited(
                            action(
                              () => badge.apply(BadgeField.brightness, [
                                badge.brightnessPresets[i],
                              ]),
                            ),
                          )
                        : null,
                  ),
                ),
              ),
          ],
        ),
        if (!badge.confirmed)
          TextButton.icon(
            onPressed: badge.busy
                ? null
                : () => unawaited(action(badge.refresh)),
            icon: const Icon(Icons.refresh),
            label: const Text('Refresh to confirm'),
          ),
      ],
    );
  }

  Future<void> openMotion() async {
    await showDialog<void>(
      context: context,
      builder: (dialog) => AlertDialog(
        title: const Text('Motion'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(dialog),
            child: const Text('Done'),
          ),
        ],
        content: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 420),
          child: ListenableBuilder(
            listenable: badge,
            builder: (_, _) => SingleChildScrollView(
              child: MotionPanel(
                framed: false,
                value:
                    badge.values[BadgeField.motion] ??
                    [1, 1, 0, 2, 3, 96, 0, 0],
                status: badge.motionStatus,
                enabled: ready,
                apply: (v) => action(() => badge.apply(BadgeField.motion, v)),
                refresh: () => action(badge.refresh),
              ),
            ),
          ),
        ),
      ),
    );
  }

  List<SavedLink> get quickLinks => [
    for (var i = 0; i < configuredLinks.length; i++)
      if (configuredLinks[i].isNotEmpty)
        SavedLink(linkLabels[i], normalizeUrl(configuredLinks[i])),
  ];
  Widget compactLinkRow(SavedLink link, {int? index}) {
    final active =
        badge.confirmed && badge.nfcVerified && badge.url == link.url;
    return SizedBox(
      height: 50,
      child: Row(
        children: [
          Icon(
            active ? Icons.radio_button_checked : Icons.link,
            size: 20,
            color: active ? Theme.of(context).colorScheme.primary : null,
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(link.label, maxLines: 1, overflow: TextOverflow.ellipsis),
                if (MediaQuery.textScalerOf(context).scale(14) <= 20)
                  Text(
                    link.url,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
              ],
            ),
          ),
          if (index != null)
            IconButton(
              tooltip: 'Edit ${link.label}',
              icon: const Icon(Icons.edit_outlined),
              onPressed: () => unawaited(editLink(index: index)),
            ),
          IconButton(
            tooltip: 'Apply ${link.label}',
            icon: Icon(active ? Icons.check : Icons.arrow_forward),
            onPressed: ready && !active
                ? () => unawaited(
                    action(
                      () => badge.apply(BadgeField.url, encodeAscii(link.url)),
                    ),
                  )
                : null,
          ),
        ],
      ),
    );
  }

  Widget compactLinks() => Column(
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      const SizedBox(height: 8),
      Row(
        children: [
          const Icon(Icons.qr_code_2, size: 30),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              badge.connected ? badge.url : 'Connect to choose a link',
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
            ),
          ),
          IconButton(
            tooltip: 'Add a custom link',
            onPressed: () => unawaited(editLink()),
            icon: const Icon(Icons.add_link),
          ),
        ],
      ),
      const Divider(),
      for (final link in quickLinks) compactLinkRow(link),
      if (quickLinks.isEmpty)
        const Padding(
          padding: EdgeInsets.all(16),
          child: Text('Add your first link'),
        ),
      if (draft.links.isNotEmpty)
        compactLinkRow(draft.links.last, index: draft.links.length - 1),
      Align(
        alignment: Alignment.centerRight,
        child: TextButton.icon(
          onPressed: openLinksLibrary,
          icon: const Icon(Icons.bookmarks_outlined, size: 18),
          label: Text('Saved links (${draft.links.length})'),
        ),
      ),
      const Spacer(),
      Row(
        children: [
          Icon(
            badge.confirmed && badge.nfcVerified
                ? Icons.nfc
                : Icons.sync_problem,
            size: 20,
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              badge.confirmed && badge.nfcVerified
                  ? 'NFC synchronized'
                  : badge.connected
                  ? 'NFC sync needed'
                  : 'NFC',
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
          ),
          IconButton(
            tooltip: 'NFC details',
            icon: const Icon(Icons.info_outline),
            onPressed: () => showDialog<void>(
              context: context,
              builder: (c) => AlertDialog(
                title: const Text('NFC'),
                content: Text(
                  badge.nfcVerified
                      ? 'QR and NFC use the same saved link. Passive taps also work with the badge off.'
                      : badge.nfcMessage,
                ),
                actions: [
                  TextButton(
                    onPressed: () => Navigator.pop(c),
                    child: const Text('Done'),
                  ),
                ],
              ),
            ),
          ),
          IconButton(
            tooltip: 'Sync again',
            icon: const Icon(Icons.sync),
            onPressed: ready && badge.capabilities.hasStudio
                ? () => unawaited(
                    action(() => badge.apply(BadgeField.nfcSync, [1])),
                  )
                : null,
          ),
        ],
      ),
    ],
  );
  Future<void> openLinksLibrary() async {
    await Navigator.push<void>(
      context,
      MaterialPageRoute(
        builder: (c) => ValueListenableBuilder<int>(
          valueListenable: libraryChanges,
          builder: (_, _, _) => Scaffold(
            appBar: AppBar(
              title: const Text('Links library'),
              actions: [
                IconButton(
                  tooltip: 'Add a custom link',
                  onPressed: () => unawaited(editLink()),
                  icon: const Icon(Icons.add),
                ),
              ],
            ),
            body: ListView(
              padding: const EdgeInsets.all(16),
              children: [
                for (var i = 0; i < draft.links.length; i++)
                  Row(
                    children: [
                      Expanded(child: compactLinkRow(draft.links[i], index: i)),
                      IconButton(
                        tooltip: 'Remove ${draft.links[i].label}',
                        icon: const Icon(Icons.delete_outline),
                        onPressed: () {
                          final index = i, removed = draft.links.removeAt(i);
                          editDraft();
                          notice(
                            '${removed.label} removed',
                            action: SnackBarAction(
                              label: 'Undo',
                              onPressed: () {
                                if (draft.links.length < 32) {
                                  draft.links.insert(
                                    index.clamp(0, draft.links.length),
                                    removed,
                                  );
                                  editDraft();
                                }
                              },
                            ),
                          );
                        },
                      ),
                    ],
                  ),
                if (draft.links.isEmpty)
                  const Center(child: Text('No saved links')),
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget compactText() {
    final keyboard = MediaQuery.viewInsetsOf(context).bottom > 0;
    final invalid = textError();
    return Column(
      children: [
        const SizedBox(height: 8),
        Flexible(
          flex: keyboard ? 1 : 0,
          fit: keyboard ? FlexFit.tight : FlexFit.loose,
          child: TextField(
            key: const Key('text-message'),
            controller: textEditor,
            focusNode: textFocus,
            selectAllOnFocus: false,
            expands: keyboard,
            textAlignVertical: TextAlignVertical.top,
            minLines: keyboard ? null : 2,
            maxLines: keyboard ? null : 3,
            maxLength: 96,
            decoration: InputDecoration(
              hintText: 'Your message',
              counterText: '',
              suffixIcon: textFocus.hasFocus
                  ? TextFieldTapRegion(
                      child: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          IconButton(
                            tooltip: 'Move cursor left',
                            icon: const Icon(Icons.chevron_left),
                            onPressed: () => moveCursor(-1),
                          ),
                          IconButton(
                            tooltip: 'Move cursor right',
                            icon: const Icon(Icons.chevron_right),
                            onPressed: () => moveCursor(1),
                          ),
                        ],
                      ),
                    )
                  : null,
              errorText: invalid == null
                  ? null
                  : 'Use ASCII text; System import supports emoji.',
            ),
            onChanged: (v) {
              draft.text = draft.text.copyWith(text: v);
              editDraft();
            },
          ),
        ),
        const SizedBox(height: 8),
        Row(
          children: [
            SelectionControl<int>(
              title: 'Text layout',
              value: draft.text.layout,
              iconOnly: true,
              choices: [
                const BadgeChoice(0, 'Auto fit', icon: Icons.fit_screen),
                const BadgeChoice(1, 'Wrap', icon: Icons.wrap_text),
                const BadgeChoice(2, 'Scroll', icon: Icons.swap_horiz),
                BadgeChoice(
                  3,
                  badge.connected && !badge.capabilities.hasEditorV2
                      ? 'Bounce lines (firmware update needed)'
                      : 'Bounce lines',
                  icon: Icons.compare_arrows,
                  enabled: !badge.connected || badge.capabilities.hasEditorV2,
                ),
              ],
              onChanged: (v) {
                draft.text = draft.text.copyWith(layout: v);
                editDraft();
              },
            ),
            SelectionControl<int>(
              title: 'Alignment',
              value: draft.text.alignment,
              iconOnly: true,
              choices: const [
                BadgeChoice(0, 'Left', icon: Icons.format_align_left),
                BadgeChoice(1, 'Center', icon: Icons.format_align_center),
                BadgeChoice(2, 'Right', icon: Icons.format_align_right),
              ],
              onChanged: (v) {
                draft.text = draft.text.copyWith(alignment: v);
                editDraft();
              },
            ),
            const Spacer(),
            SizedBox(
              width: 112,
              child: SelectionControl<int>(
                key: const Key('text-size'),
                title: 'Text size',
                subtle: true,
                value: draft.text.scale,
                choices: [
                  for (var i = 0; i < 4; i++)
                    BadgeChoice(i, i == 0 ? 'Auto' : '${i}x'),
                ],
                onChanged: (v) {
                  draft.text = draft.text.copyWith(scale: v);
                  editDraft();
                },
              ),
            ),
            IconButton(
              tooltip: 'Text speed',
              icon: const Icon(Icons.speed),
              onPressed: () => openTextSpeed(),
            ),
          ],
        ),
        if (!keyboard)
          Expanded(
            child: invalid != null
                ? const Center(child: Icon(Icons.text_fields, size: 48))
                : Padding(
                    padding: const EdgeInsets.symmetric(vertical: 8),
                    child: canvasSpace(
                      preview(
                        (t) => draft.text.render(
                          t,
                          fallback: badge.name.isEmpty ? 'BADGE' : badge.name,
                        ),
                        animate: true,
                      ),
                    ),
                  ),
          ),
        Row(
          children: [
            IconButton(
              tooltip: 'Load from badge',
              onPressed:
                  badge.confirmed && !badge.busy && badge.capabilities.hasStudio
                  ? () => unawaited(action(() => loadBadgeDraft(text: true)))
                  : null,
              icon: const Icon(Icons.download_outlined),
            ),
            IconButton(
              tooltip: 'Save to library',
              onPressed: invalid == null
                  ? () => unawaited(action(() => saveToLibrary(true)))
                  : null,
              icon: const Icon(Icons.bookmark_add_outlined),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: FilledButton.icon(
                style: FilledButton.styleFrom(
                  padding: const EdgeInsets.symmetric(horizontal: 12),
                ),
                onPressed: studioReady && invalid == null
                    ? () => unawaited(
                        action(() async {
                          await badge.apply(
                            BadgeField.text,
                            draft.text.encode(),
                          );
                          await badge.apply(BadgeField.mode, [1]);
                        }),
                      )
                    : null,
                icon: const Icon(Icons.send_outlined),
                label: const Text('Send text'),
              ),
            ),
          ],
        ),
      ],
    );
  }

  Future<void> openTextSpeed() async {
    await showDialog<void>(
      context: context,
      builder: (c) => StatefulBuilder(
        builder: (_, refresh) => AlertDialog(
          title: const Text('Text speed'),
          content: SizedBox(
            width: 300,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Slider(
                  min: 50,
                  max: 500,
                  value: draft.text.interval.toDouble(),
                  label: '${draft.text.interval} ms',
                  onChanged: (v) {
                    draft.text = draft.text.copyWith(interval: v.round());
                    editDraft();
                    refresh(() {});
                  },
                ),
                Text('${draft.text.interval} ms'),
              ],
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(c),
              child: const Text('Done'),
            ),
          ],
        ),
      ),
    );
  }

  Widget compactStudio() {
    final design = draft.matrix;
    return Column(
      children: [
        Row(
          children: [
            Tooltip(
              message:
                  'Frame ${design.selected + 1} of ${design.frames.length}',
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 80),
                child: Text(
                  '${design.selected + 1}/${design.frames.length}',
                  style: Theme.of(context).textTheme.labelLarge,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
              ),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: Text(
                '${(artworkBudgetBytes / 1024).toStringAsFixed(1)} / ${(maxArtworkBytes / 1024).toStringAsFixed(1)} KiB',
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: Theme.of(context).textTheme.labelSmall,
              ),
            ),
            IconButton(
              tooltip: 'Load from badge',
              icon: const Icon(Icons.download_outlined),
              onPressed:
                  badge.confirmed && !badge.busy && badge.capabilities.hasStudio
                  ? () => unawaited(action(() => loadBadgeDraft(text: false)))
                  : null,
            ),
            IconButton(
              tooltip: playing ? 'Pause preview' : 'Play preview',
              icon: Icon(
                playing
                    ? Icons.pause_circle_outline
                    : Icons.visibility_outlined,
              ),
              onPressed: () => update(() {
                if (!playing) playEpoch = watch.elapsedMilliseconds;
                playing = !playing;
              }),
            ),
          ],
        ),
        Expanded(
          child: canvasSpace(
            Stack(
              children: [
                Positioned.fill(
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(20),
                    child: InteractiveViewer(
                      panEnabled: navigateCanvas,
                      scaleEnabled: navigateCanvas,
                      minScale: 1,
                      maxScale: 8,
                      child: playing
                          ? ValueListenableBuilder<int>(
                              valueListenable: clock,
                              builder: (_, t, _) => MatrixPreview(
                                frame: design
                                    .frames[design.frameAt(t - playEpoch)],
                              ),
                            )
                          : MatrixEditor(
                              design: design,
                              tool: tool,
                              onChanged: () => editDraft(pixelsOnly: true),
                              onStrokeStart: () => drawing = true,
                              onStrokeEnd: finishStroke,
                              enabled: !navigateCanvas && !loadingDraft,
                              onion: onionSkin && design.selected > 0
                                  ? design.frames[design.selected - 1]
                                  : null,
                              onionOpacity: onionOpacity,
                            ),
                    ),
                  ),
                ),
                Positioned(
                  left: 4,
                  bottom: 4,
                  child: IconButton.filledTonal(
                    tooltip: 'Pan and zoom',
                    isSelected: navigateCanvas,
                    icon: Icon(
                      navigateCanvas
                          ? Icons.zoom_out_map
                          : Icons.pan_tool_outlined,
                    ),
                    onPressed: () =>
                        update(() => navigateCanvas = !navigateCanvas),
                  ),
                ),
                Positioned(
                  right: 4,
                  bottom: 4,
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      IconButton.filledTonal(
                        tooltip: 'Undo',
                        icon: const Icon(Icons.undo),
                        onPressed: design.canUndo
                            ? () {
                                design.undo();
                                editDraft();
                              }
                            : null,
                      ),
                      const SizedBox(width: 4),
                      IconButton.filledTonal(
                        tooltip: 'Redo',
                        icon: const Icon(Icons.redo),
                        onPressed: design.canRedo
                            ? () {
                                design.redo();
                                editDraft();
                              }
                            : null,
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ),
        Row(
          children: [
            SelectionControl<int>(
              title: 'Brush size',
              value: design.brushSize,
              iconOnly: true,
              choices: const [
                BadgeChoice(1, '1 px', icon: Icons.circle),
                BadgeChoice(2, '2 px', icon: Icons.circle),
                BadgeChoice(3, '3 px', icon: Icons.circle),
              ],
              onChanged: (v) => update(() => design.brushSize = v),
            ),
            Expanded(
              child: Slider(
                min: 0,
                max: 255,
                divisions: 255,
                value: design.brush.toDouble(),
                onChanged: (v) => update(() => design.brush = v.round()),
              ),
            ),
            SizedBox(
              width: 34,
              child: Text(
                '${design.brush}',
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.labelMedium,
              ),
            ),
            IconButton(
              tooltip: 'Pick brightness',
              icon: const Icon(Icons.colorize),
              isSelected: tool == PixelTool.picker,
              onPressed: () => update(() {
                tool = PixelTool.picker;
                navigateCanvas = false;
              }),
            ),
            IconButton(
              tooltip: 'Fill',
              icon: const Icon(Icons.format_color_fill),
              isSelected: tool == PixelTool.fill,
              onPressed: () => update(() {
                tool = PixelTool.fill;
                navigateCanvas = false;
              }),
            ),
          ],
        ),
        Row(
          children: [
            IconButton(
              tooltip: playing ? 'Pause animation' : 'Play animation',
              icon: Icon(playing ? Icons.pause : Icons.play_arrow),
              onPressed: () => update(() {
                if (!playing) playEpoch = watch.elapsedMilliseconds;
                playing = !playing;
              }),
            ),
            SizedBox(
              width: 72,
              child: TextButton(
                onPressed: openTimelineSettings,
                child: Text('${design.durations[design.selected]} ms'),
              ),
            ),
            const Spacer(),
            IconButton(
              tooltip: 'Add frame',
              onPressed: design.frames.length < maxFrames
                  ? () {
                      design.add();
                      editDraft();
                    }
                  : null,
              icon: const Icon(Icons.add),
            ),
            IconButton(
              tooltip: 'Duplicate frame',
              onPressed: design.frames.length < maxFrames
                  ? () {
                      design.add(duplicate: true);
                      editDraft();
                    }
                  : null,
              icon: const Icon(Icons.copy),
            ),
            IconButton(
              tooltip: 'Delete frame',
              onPressed: design.frames.length > 1
                  ? () {
                      design.remove();
                      editDraft();
                    }
                  : null,
              icon: const Icon(Icons.delete_outline),
            ),
          ],
        ),
        SizedBox(
          height: 48,
          child: ReorderableListView.builder(
            scrollDirection: Axis.horizontal,
            buildDefaultDragHandles: false,
            itemCount: design.frames.length,
            onReorder: (old, next) {
              design.checkpoint();
              if (next > old) next--;
              final pixels = design.frames.removeAt(old),
                  duration = design.durations.removeAt(old);
              design.frames.insert(next, pixels);
              design.durations.insert(next, duration);
              design.selected = next;
              editDraft();
            },
            itemBuilder: (c, i) => ReorderableDelayedDragStartListener(
              key: ValueKey(design.frames[i]),
              index: i,
              child: Padding(
                padding: const EdgeInsets.only(right: 6),
                child: GestureDetector(
                  onTap: () => update(() {
                    design.selected = i;
                    playing = false;
                  }),
                  child: Container(
                    width: 46,
                    padding: const EdgeInsets.all(3),
                    decoration: BoxDecoration(
                      border: Border.all(
                        color: design.selected == i
                            ? Theme.of(context).colorScheme.primary
                            : Theme.of(context).dividerColor,
                        width: design.selected == i ? 2 : 1,
                      ),
                      borderRadius: BorderRadius.circular(6),
                    ),
                    child: MatrixPreview(frame: design.frames[i]),
                  ),
                ),
              ),
            ),
          ),
        ),
        Row(
          children: [
            for (final item in [
              (Icons.brush_outlined, 'Brush'),
              (Icons.auto_fix_high_outlined, 'Eraser'),
              (Icons.select_all, 'Select'),
              (Icons.auto_awesome_outlined, 'Actions'),
              (Icons.add_photo_alternate_outlined, 'Media'),
            ])
              Expanded(
                child: IconButton(
                  tooltip: item.$2,
                  isSelected:
                      studioToolPage ==
                      [
                        'Brush',
                        'Eraser',
                        'Select',
                        'Actions',
                        'Media',
                      ].indexOf(item.$2),
                  icon: Icon(item.$1),
                  onPressed: () => chooseStudioTool(item.$2),
                ),
              ),
            IconButton.filled(
              tooltip: 'Send design',
              onPressed: studioReady && artworkHasPixels
                  ? () => unawaited(action(sendStudio))
                  : null,
              icon: const Icon(Icons.send_outlined),
            ),
          ],
        ),
      ],
    );
  }

  Future<void> sendStudio() async {
    final bytes = draft.matrix.encode(forBadge: true);
    await badge.apply(BadgeField.artwork, bytes);
    await badge.apply(BadgeField.mode, [bytes[1] > 1 ? 3 : 2]);
  }

  void chooseStudioTool(String category) {
    update(() {
      studioToolPage = [
        'Brush',
        'Eraser',
        'Select',
        'Actions',
        'Media',
      ].indexOf(category);
      playing = false;
      navigateCanvas = false;
    });
    if (category == 'Eraser') {
      update(() => tool = PixelTool.erase);
      return;
    }
    if (category == 'Select') {
      update(() => tool = PixelTool.select);
      return;
    }
    if (category == 'Brush') {
      update(() => tool = PixelTool.pen);
      openBrushTools();
      return;
    }
    if (category == 'Actions') {
      openStudioActions();
      return;
    }
    openMediaTools();
  }

  Future<void> toolSheet(List<(IconData, String, VoidCallback)> tools) async {
    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      isScrollControlled: true,
      builder: (sheet) => SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(12, 0, 12, 16),
          child: LayoutBuilder(
            builder: (context, constraints) {
              final largeText = MediaQuery.textScalerOf(context).scale(14) > 20;
              final columns = largeText ? 2 : 3;
              final width =
                  (constraints.maxWidth - (columns - 1) * 8) / columns;
              return SingleChildScrollView(
                child: Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    for (final entry in tools)
                      SizedBox(
                        width: width,
                        height: largeText ? 112 : 80,
                        child: TextButton(
                          style: TextButton.styleFrom(
                            backgroundColor: Theme.of(
                              context,
                            ).colorScheme.surfaceContainerLow,
                            foregroundColor: Theme.of(
                              context,
                            ).colorScheme.onSurface,
                            shape: RoundedRectangleBorder(
                              borderRadius: BorderRadius.circular(12),
                              side: BorderSide(
                                color: Theme.of(context)
                                    .colorScheme
                                    .outlineVariant
                                    .withValues(alpha: .5),
                              ),
                            ),
                            padding: const EdgeInsets.symmetric(
                              horizontal: 4,
                              vertical: 8,
                            ),
                          ),
                          onPressed: () {
                            Navigator.pop(sheet);
                            entry.$3();
                          },
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Icon(entry.$1, size: 22),
                              const SizedBox(height: 6),
                              Text(
                                entry.$2,
                                textAlign: TextAlign.center,
                                maxLines: 2,
                                style: Theme.of(context).textTheme.labelMedium,
                              ),
                            ],
                          ),
                        ),
                      ),
                  ],
                ),
              );
            },
          ),
        ),
      ),
    );
  }

  void openBrushTools() => unawaited(
    toolSheet([
      for (final item in [
        (PixelTool.pen, Icons.brush, 'Freehand'),
        (PixelTool.line, Icons.show_chart, 'Line'),
        (PixelTool.rectangle, Icons.crop_square, 'Rectangle'),
        (PixelTool.ellipse, Icons.circle_outlined, 'Ellipse'),
      ])
        (item.$2, item.$3, () => update(() => tool = item.$1)),
    ]),
  );
  Future<void> newDesign() async {
    if (artworkHasPixels || draft.matrix.frames.length > 1) {
      final yes = await showDialog<bool>(
        context: context,
        builder: (c) => AlertDialog(
          title: const Text('Start a new design?'),
          content: Text(
            'Replace all ${draft.matrix.frames.length} frames in this editor. Saved library designs stay available.',
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(c, false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(c, true),
              child: const Text('New design'),
            ),
          ],
        ),
      );
      if (yes != true || !mounted) return;
    }
    draft.matrix = MatrixDesign();
    playing = false;
    navigateCanvas = false;
    onionSkin = false;
    tool = PixelTool.pen;
    studioToolPage = 0;
    editDraft();
  }

  void openStudioActions() {
    final d = draft.matrix;
    void edit(VoidCallback operation) {
      operation();
      editDraft();
    }

    unawaited(
      toolSheet([
        (Icons.note_add_outlined, 'New design', () => unawaited(newDesign())),
        (
          Icons.copy,
          'Copy',
          () {
            d.copySelection();
            update(() {});
          },
        ),
        (Icons.cut, 'Cut', () => edit(() => d.copySelection(cut: true))),
        (Icons.paste, 'Paste', () => edit(d.pasteSelection)),
        (Icons.rotate_90_degrees_cw, 'Rotate', () => edit(d.rotate)),
        (Icons.flip, 'Mirror', () => edit(() => d.flip(horizontal: true))),
        (Icons.swap_vert, 'Flip', () => edit(() => d.flip(horizontal: false))),
        (Icons.invert_colors, 'Invert', () => edit(d.invert)),
        (Icons.clear, 'Clear frame', () => edit(d.clear)),
        (
          Icons.layers_outlined,
          'Onion skin',
          () => update(() => onionSkin = !onionSkin),
        ),
        (
          Icons.bookmark_add_outlined,
          'Save to library',
          () => unawaited(action(() => saveToLibrary(false))),
        ),
      ]),
    );
  }

  void openMediaTools() => unawaited(
    toolSheet([
      (
        Icons.image_outlined,
        'Image / GIF',
        () => unawaited(action(importImage)),
      ),
      (
        Icons.movie_outlined,
        'Video',
        () => unawaited(action(() => importStudioSource(video: true))),
      ),
      (
        Icons.text_fields,
        'Text / Emoji',
        () => unawaited(action(() => importStudioSource(text: true))),
      ),
      (
        Icons.data_object,
        'Import data',
        () => unawaited(action(() => importStudioSource())),
      ),
      (
        Icons.copy_all,
        'Copy data',
        () => unawaited(action(() => copyStudioData(draft.matrix))),
      ),
      (
        Icons.save_alt,
        'Export data',
        () => unawaited(action(() => exportStudio(draft.matrix))),
      ),
      (
        Icons.image,
        'Export PNG',
        () => unawaited(action(() => exportStudio(draft.matrix, png: true))),
      ),
      (
        Icons.content_paste,
        'Paste image',
        () => unawaited(
          action(() async {
            if (importing) return;
            update(() => importing = true);
            try {
              final image = await pasteStudioImage();
              if (image == null || !mounted) return;
              final next = await showDialog<MatrixDesign>(
                context: context,
                useSafeArea: false,
                builder: (_) => ImportDesignDialog(image: image),
              );
              if (next != null && mounted) {
                draft.matrix = next;
                editDraft();
              }
            } finally {
              update(() => importing = false);
            }
          }),
        ),
      ),
    ]),
  );
  Future<void> openTimelineSettings() async {
    await showDialog<void>(
      context: context,
      builder: (c) => StatefulBuilder(
        builder: (_, refresh) {
          final d = draft.matrix;
          return AlertDialog(
            title: const Text('Animation'),
            content: SizedBox(
              width: 320,
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  SelectionControl<int>(
                    title: 'Playback',
                    value: d.playback,
                    expand: true,
                    choices: const [
                      BadgeChoice(0, 'Loop', icon: Icons.repeat),
                      BadgeChoice(1, 'Once', icon: Icons.play_circle_outline),
                      BadgeChoice(
                        2,
                        'Reverse',
                        icon: Icons.keyboard_double_arrow_left,
                      ),
                      BadgeChoice(3, 'Ping-pong', icon: Icons.compare_arrows),
                    ],
                    onChanged: (v) {
                      d.checkpoint();
                      d.playback = v;
                      editDraft();
                      refresh(() {});
                    },
                  ),
                  Slider(
                    min: 50,
                    max: 2000,
                    value: d.durations[d.selected].toDouble(),
                    label: '${d.durations[d.selected]} ms',
                    onChanged: (v) {
                      d.durations[d.selected] = v.round();
                      editDraft();
                      refresh(() {});
                    },
                  ),
                  Text('${d.durations[d.selected]} ms'),
                  TextButton(
                    onPressed: () {
                      d.interval = d.durations[d.selected];
                      editDraft();
                      refresh(() {});
                    },
                    child: const Text('Use for all frames'),
                  ),
                ],
              ),
            ),
            actions: [
              TextButton(
                onPressed: () => Navigator.pop(c),
                child: const Text('Done'),
              ),
            ],
          );
        },
      ),
    );
  }

  Future<void> openDesignLibrary(bool text) async {
    await Navigator.push<void>(
      context,
      MaterialPageRoute(
        builder: (c) => ValueListenableBuilder<int>(
          valueListenable: libraryChanges,
          builder: (_, _, _) => Scaffold(
            appBar: AppBar(
              title: Text(text ? 'Text library' : 'Design library'),
              actions: [
                IconButton(
                  tooltip: 'Save to library',
                  icon: const Icon(Icons.bookmark_add_outlined),
                  onPressed: () => unawaited(action(() => saveToLibrary(text))),
                ),
              ],
            ),
            body: ListView(
              padding: const EdgeInsets.all(16),
              children: [libraryPanel(text)],
            ),
          ),
        ),
      ),
    );
  }

  Widget compactAudio(BuildContext context) => AudioPage(
    audio: audio,
    compact: true,
    recordBlocked: badge.busy,
    openLibrary: () => openRecordings(context),
    selectVisualization: (mode) =>
        action(() => badge.apply(BadgeField.mode, [mode])),
  );
  Future<void> openRecordings(BuildContext context) async {
    await Navigator.push<void>(
      context,
      MaterialPageRoute(
        builder: (c) => ListenableBuilder(
          listenable: audio,
          builder: (_, _) => Scaffold(
            appBar: AppBar(
              title: const Text('Recordings'),
              actions: [
                IconButton(
                  tooltip: 'Refresh recordings',
                  onPressed: audio.busy || audio.recording || audio.stopping
                      ? null
                      : () => unawaited(
                          audio.refreshLibrary().catchError((Object _) {}),
                        ),
                  icon: const Icon(Icons.refresh),
                ),
              ],
            ),
            body: ListView(
              padding: const EdgeInsets.all(12),
              children: [
                if (audio.recordings.isEmpty)
                  const Padding(
                    padding: EdgeInsets.all(24),
                    child: Center(child: Text('No recordings yet.')),
                  ),
                for (final item in audio.recordings)
                  AudioPage(
                    audio: audio,
                    selectVisualization: (_) async {},
                  ).recordingTile(c, item),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
