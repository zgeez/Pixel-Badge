// SPDX-License-Identifier: MIT
import 'package:flutter/material.dart';

class BadgeChoice<T> {
  const BadgeChoice(this.value, this.label, {this.icon, this.enabled = true});
  final T value;
  final String label;
  final IconData? icon;
  final bool enabled;
}

/// A controlled selector with a single, accessible choice sheet on every page.
class SelectionControl<T> extends StatelessWidget {
  const SelectionControl({
    super.key,
    required this.title,
    required this.value,
    required this.choices,
    required this.onChanged,
    this.iconOnly = false,
    this.expand = false,
    this.subtle = false,
  });
  final String title;
  final T value;
  final List<BadgeChoice<T>> choices;
  final ValueChanged<T>? onChanged;
  final bool iconOnly, expand, subtle;

  Future<void> choose(BuildContext context) async {
    FocusManager.instance.primaryFocus?.unfocus();
    final selected = await showModalBottomSheet<T>(
      context: context,
      useSafeArea: true,
      isScrollControlled: true,
      showDragHandle: true,
      constraints: const BoxConstraints(maxWidth: 520),
      builder: (sheet) {
        final colors = Theme.of(sheet).colorScheme;
        return SafeArea(
          top: false,
          child: ConstrainedBox(
            constraints: BoxConstraints(
              maxHeight: MediaQuery.sizeOf(sheet).height * .72,
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(20, 0, 8, 8),
                  child: Row(
                    children: [
                      Expanded(
                        child: Text(
                          title,
                          style: Theme.of(sheet).textTheme.titleMedium,
                        ),
                      ),
                      IconButton(
                        tooltip: 'Close $title',
                        icon: const Icon(Icons.close, size: 20),
                        onPressed: () => Navigator.pop(sheet),
                      ),
                    ],
                  ),
                ),
                Flexible(
                  child: SingleChildScrollView(
                    padding: const EdgeInsets.fromLTRB(12, 0, 12, 16),
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        for (final choice in choices)
                          Padding(
                            padding: const EdgeInsets.symmetric(vertical: 2),
                            child: Material(
                              color: choice.value == value
                                  ? colors.secondaryContainer
                                  : Colors.transparent,
                              borderRadius: BorderRadius.circular(12),
                              clipBehavior: Clip.antiAlias,
                              child: ListTile(
                                selected: choice.value == value,
                                selectedTileColor: Colors.transparent,
                                selectedColor: colors.onSecondaryContainer,
                                shape: RoundedRectangleBorder(
                                  borderRadius: BorderRadius.circular(12),
                                ),
                                minVerticalPadding: 12,
                                leading: choice.icon == null
                                    ? null
                                    : Icon(choice.icon, size: 22),
                                title: Text(choice.label),
                                trailing: choice.value == value
                                    ? const Icon(Icons.check, size: 20)
                                    : null,
                                enabled: choice.enabled,
                                onTap: choice.enabled
                                    ? () => Navigator.pop(sheet, choice.value)
                                    : null,
                              ),
                            ),
                          ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ),
        );
      },
    );
    if (context.mounted && selected != null && selected != value) {
      onChanged?.call(selected);
    }
  }

  @override
  Widget build(BuildContext context) {
    final choice = choices.firstWhere((choice) => choice.value == value);
    final colors = Theme.of(context).colorScheme;
    return Tooltip(
      message: title,
      child: TextButton(
        onPressed: onChanged == null ? null : () => choose(context),
        style: TextButton.styleFrom(
          foregroundColor: colors.onSurface,
          backgroundColor: iconOnly || subtle
              ? null
              : colors.surfaceContainerHigh,
          minimumSize: const Size(48, 48),
          padding: EdgeInsets.symmetric(horizontal: iconOnly ? 12 : 14),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(12),
          ),
        ),
        child: iconOnly
            ? Icon(choice.icon ?? Icons.tune, size: 22)
            : Row(
                mainAxisSize: expand ? MainAxisSize.max : MainAxisSize.min,
                children: [
                  if (choice.icon != null) ...[
                    Icon(choice.icon, size: 20),
                    const SizedBox(width: 10),
                  ],
                  if (expand)
                    Expanded(
                      child: Text(
                        choice.label,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                      ),
                    )
                  else
                    Flexible(
                      child: Text(
                        choice.label,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  const SizedBox(width: 8),
                  Icon(
                    Icons.expand_more,
                    size: 18,
                    color: colors.onSurfaceVariant,
                  ),
                ],
              ),
      ),
    );
  }
}

class MenuActionLabel extends StatelessWidget {
  const MenuActionLabel(
    this.icon,
    this.label, {
    super.key,
    this.destructive = false,
  });
  final IconData icon;
  final String label;
  final bool destructive;
  @override
  Widget build(BuildContext context) {
    final color = destructive ? Theme.of(context).colorScheme.error : null;
    return Row(
      children: [
        Icon(icon, size: 20, color: color),
        const SizedBox(width: 12),
        Text(label, style: TextStyle(color: color)),
      ],
    );
  }
}
