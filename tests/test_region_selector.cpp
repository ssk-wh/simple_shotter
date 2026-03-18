#include <QtTest>
#include <QSignalSpy>
#include "core/region_selector.h"

using namespace simpleshotter;

class TestRegionSelector : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        m_selector = std::make_unique<RegionSelector>();
    }

    void cleanup()
    {
        m_selector.reset();
    }

    // --- beginSelection / updateSelection / endSelection ---

    void test_beginSelection_setsStartPoint()
    {
        m_selector->beginSelection(QPoint(100, 200));
        QVERIFY(m_selector->isSelecting());
        QCOMPARE(m_selector->rect().topLeft(), QPoint(100, 200));
    }

    void test_updateSelection_updatesRect()
    {
        QSignalSpy spy(m_selector.get(), &RegionSelector::selectionChanged);
        m_selector->beginSelection(QPoint(10, 20));
        m_selector->updateSelection(QPoint(110, 120));

        QRect r = m_selector->rect();
        QVERIFY(r.width() > 0);
        QVERIFY(r.height() > 0);
        QVERIFY(spy.count() >= 1);
    }

    void test_updateSelection_ignoredWhenNotSelecting()
    {
        QSignalSpy spy(m_selector.get(), &RegionSelector::selectionChanged);
        m_selector->updateSelection(QPoint(50, 50));
        QCOMPARE(spy.count(), 0);
    }

    void test_endSelection_stopsSelecting()
    {
        QSignalSpy spy(m_selector.get(), &RegionSelector::selectionFinished);
        m_selector->beginSelection(QPoint(0, 0));
        m_selector->updateSelection(QPoint(100, 100));
        m_selector->endSelection();

        QVERIFY(!m_selector->isSelecting());
        QCOMPARE(spy.count(), 1);
    }

    void test_endSelection_emitsNormalizedRect()
    {
        QSignalSpy spy(m_selector.get(), &RegionSelector::selectionFinished);
        m_selector->beginSelection(QPoint(100, 100));
        m_selector->updateSelection(QPoint(50, 50));
        m_selector->endSelection();

        QRect emitted = spy.first().first().toRect();
        QVERIFY(emitted.width() > 0);
        QVERIFY(emitted.height() > 0);
    }

    // --- normalizedRect ---

    void test_normalizedRect_ensuresPositiveSize()
    {
        // Drag from bottom-right to top-left: raw rect may have negative size
        m_selector->beginSelection(QPoint(200, 200));
        m_selector->updateSelection(QPoint(50, 50));

        QRect nr = m_selector->normalizedRect();
        QVERIFY(nr.width() > 0);
        QVERIFY(nr.height() > 0);
        QCOMPARE(nr.topLeft(), QPoint(50, 50));
    }

    // --- hitTest ---

    void test_hitTest_body()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QCOMPARE(m_selector->hitTest(QPoint(200, 200)), RegionSelector::Handle::Body);
    }

    void test_hitTest_none_outsideRect()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QCOMPARE(m_selector->hitTest(QPoint(0, 0)), RegionSelector::Handle::None);
    }

    void test_hitTest_topLeftHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        // TopLeft handle is centered at (100, 100)
        QCOMPARE(m_selector->hitTest(QPoint(100, 100)), RegionSelector::Handle::TopLeft);
    }

    void test_hitTest_topHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        // Top handle is centered at (center_x, 100)
        QRect r = m_selector->normalizedRect();
        QCOMPARE(m_selector->hitTest(QPoint(r.center().x(), 100)),
                 RegionSelector::Handle::Top);
    }

    void test_hitTest_topRightHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect r = m_selector->normalizedRect();
        QCOMPARE(m_selector->hitTest(QPoint(r.right(), r.top())),
                 RegionSelector::Handle::TopRight);
    }

    void test_hitTest_leftHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect r = m_selector->normalizedRect();
        QCOMPARE(m_selector->hitTest(QPoint(r.left(), r.center().y())),
                 RegionSelector::Handle::Left);
    }

    void test_hitTest_rightHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect r = m_selector->normalizedRect();
        QCOMPARE(m_selector->hitTest(QPoint(r.right(), r.center().y())),
                 RegionSelector::Handle::Right);
    }

    void test_hitTest_bottomLeftHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect r = m_selector->normalizedRect();
        QCOMPARE(m_selector->hitTest(QPoint(r.left(), r.bottom())),
                 RegionSelector::Handle::BottomLeft);
    }

    void test_hitTest_bottomHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect r = m_selector->normalizedRect();
        QCOMPARE(m_selector->hitTest(QPoint(r.center().x(), r.bottom())),
                 RegionSelector::Handle::Bottom);
    }

    void test_hitTest_bottomRightHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect r = m_selector->normalizedRect();
        QCOMPARE(m_selector->hitTest(QPoint(r.right(), r.bottom())),
                 RegionSelector::Handle::BottomRight);
    }

    // --- handleRects ---

    void test_handleRects_returns8Rects()
    {
        m_selector->setRect(QRect(50, 50, 300, 200));
        auto handles = m_selector->handleRects();
        QCOMPARE(static_cast<int>(handles.size()), 8);
        for (const auto& hr : handles) {
            QVERIFY(hr.width() > 0);
            QVERIFY(hr.height() > 0);
        }
    }

    // --- beginResize / updateResize ---

    void test_beginResize_updateResize_movesBody()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect original = m_selector->normalizedRect();

        m_selector->beginResize(RegionSelector::Handle::Body, QPoint(200, 200));
        m_selector->updateResize(QPoint(220, 230));

        QRect moved = m_selector->rect();
        QCOMPARE(moved.topLeft(), original.topLeft() + QPoint(20, 30));
        QCOMPARE(moved.size(), original.size());
    }

    void test_beginResize_updateResize_resizesRight()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect original = m_selector->normalizedRect();

        m_selector->beginResize(RegionSelector::Handle::Right, QPoint(300, 200));
        m_selector->updateResize(QPoint(350, 200));

        QRect resized = m_selector->rect();
        QCOMPARE(resized.right(), original.right() + 50);
        QCOMPARE(resized.top(), original.top());
    }

    void test_beginResize_updateResize_resizesTopLeft()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));

        m_selector->beginResize(RegionSelector::Handle::TopLeft, QPoint(100, 100));
        m_selector->updateResize(QPoint(80, 80));

        QRect resized = m_selector->rect();
        QCOMPARE(resized.topLeft(), QPoint(80, 80));
    }

    void test_updateResize_ignoredWhenNoActiveHandle()
    {
        m_selector->setRect(QRect(100, 100, 200, 200));
        QRect before = m_selector->rect();
        m_selector->updateResize(QPoint(999, 999));
        QCOMPARE(m_selector->rect(), before);
    }

    // --- setRect ---

    void test_setRect_emitsSelectionChanged()
    {
        QSignalSpy spy(m_selector.get(), &RegionSelector::selectionChanged);
        m_selector->setRect(QRect(10, 20, 300, 400));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(m_selector->rect(), QRect(10, 20, 300, 400));
    }

    // --- Boundary conditions ---

    void test_zeroSizeSelection()
    {
        m_selector->beginSelection(QPoint(100, 100));
        m_selector->updateSelection(QPoint(100, 100));

        QRect nr = m_selector->normalizedRect();
        // Zero-size is valid (normalized), just ensure no crash
        QVERIFY(nr.width() >= 0);
        QVERIFY(nr.height() >= 0);
    }

    void test_negativeCoordinates()
    {
        m_selector->beginSelection(QPoint(-50, -30));
        m_selector->updateSelection(QPoint(100, 100));

        QRect nr = m_selector->normalizedRect();
        QCOMPARE(nr.left(), -50);
        QCOMPARE(nr.top(), -30);
        QVERIFY(nr.width() > 0);
        QVERIFY(nr.height() > 0);
    }

    void test_reverseDirectionSelection()
    {
        // Select from bottom-right to top-left
        m_selector->beginSelection(QPoint(500, 500));
        m_selector->updateSelection(QPoint(100, 100));

        QRect nr = m_selector->normalizedRect();
        QCOMPARE(nr.topLeft(), QPoint(100, 100));
        QVERIFY(nr.width() > 0);
        QVERIFY(nr.height() > 0);
    }

private:
    std::unique_ptr<RegionSelector> m_selector;
};

QTEST_MAIN(TestRegionSelector)
#include "test_region_selector.moc"
