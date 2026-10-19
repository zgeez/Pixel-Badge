// SPDX-License-Identifier: MIT
import 'package:badge_app/selection_control.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'screen_test.dart' show capture, loadUiFonts;
import 'visual_review_test.dart' show surface, click;

void main() {
  setUpAll(loadUiFonts);
  testWidgets(
    'selection chooses false and cancel preserves the current value',
    (tester) async {
      var value = true, calls = 0;
      await surface(
        tester,
        Dialog(
          child: StatefulBuilder(
            builder: (context, rebuild) => SelectionControl<bool>(
              title: 'Source type',
              value: value,
              choices: const [
                BadgeChoice(true, 'Glyph'),
                BadgeChoice(false, 'Photo'),
              ],
              onChanged: (next) => rebuild(() {
                value = next;
                calls++;
              }),
            ),
          ),
        ),
        Brightness.light,
      );
      await click(tester, find.byTooltip('Source type'));
      await click(tester, find.text('Photo'));
      expect(value, isFalse);
      expect(calls, 1);
      await click(tester, find.byTooltip('Source type'));
      await click(tester, find.byTooltip('Close Source type'));
      expect(value, isFalse);
      expect(calls, 1);
      await tester.pumpWidget(const SizedBox());
    },
  );

  testWidgets(
    'long choice sheets fit large text and allow selecting the final option',
    (tester) async {
      int? selected;
      await surface(
        tester,
        Dialog(
          child: SelectionControl<int>(
            title: 'Display mode',
            value: 0,
            expand: true,
            choices: [
              for (var i = 0; i < 7; i++)
                BadgeChoice(
                  i,
                  [
                    'QR link',
                    'Text',
                    'Drawing',
                    'Animation',
                    'Audio level',
                    'Audio spectrum',
                    'Fluid',
                  ][i],
                ),
            ],
            onChanged: (next) => selected = next,
          ),
        ),
        Brightness.dark,
        width: 320,
        height: 640,
        scale: 2,
      );
      await click(tester, find.byTooltip('Display mode'));
      await capture(tester, 'review-narrow-choice-sheet');
      await tester.ensureVisible(find.text('Fluid'));
      await tester.pump();
      await capture(tester, 'review-narrow-choice-sheet-bottom');
      await click(tester, find.text('Fluid'));
      expect(selected, 6);
      expect(tester.takeException(), isNull);
      await tester.pumpWidget(const SizedBox());
    },
  );
}
