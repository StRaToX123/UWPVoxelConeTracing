#include "imCurveEditor.h"



namespace ImGui
{
    

    // [src] http://iquilezles.org/www/articles/minispline/minispline.htm
    // key format (for dim == 1) is (t0,x0,t1,x1 ...)
    // key format (for dim == 2) is (t0,x0,y0,t1,x1,y1 ...)
    // key format (for dim == 3) is (t0,x0,y0,z0,t1,x1,y1,z1 ...)
    template<int DIM>
    void spline(const float* key, int num, float t, float* v)
    {
        static float coefs[16] = {
            -1.0f, 2.0f,-1.0f, 0.0f,
                3.0f,-5.0f, 0.0f, 2.0f,
            -3.0f, 4.0f, 1.0f, 0.0f,
                1.0f,-1.0f, 0.0f, 0.0f
        };

        const int size = DIM + 1;

        // find key
        int k = 0;
        while (key[k * size] < t)
            k++;

        const float key0 = key[(k - 1) * size];
        const float key1 = key[k * size];

        // interpolant
        const float h = (t - key0) / (key1 - key0);

        // init result
        for (int i = 0; i < DIM; i++)
            v[i] = 0.0f;

        // add basis functions
        for (int i = 0; i < 4; ++i)
        {
            const float* co = &coefs[4 * i];
            const float b = 0.5f * (((co[0] * h + co[1]) * h + co[2]) * h + co[3]);

            const int kn = ImClamp(k + i - 2, 0, num - 1);
            for (int j = 0; j < DIM; j++)
                v[j] += b * key[kn * size + j + 1];
        }
    }

    float CurveValueSmooth(float p, int maxpoints, const ImVec2* points)
    {
        if (maxpoints < 2 || points == 0)
            return 0;
        if (p < 0)
            return points[0].y;

        float* input = new float[maxpoints * 2];
        float output[4];

        for (int i = 0; i < maxpoints; ++i)
        {
            input[i * 2 + 0] = points[i].x;
            input[i * 2 + 1] = points[i].y;
        }

        spline<1>(input, maxpoints, p, output);

        delete[] input;
        return output[0];
    }

    float CurveValue(float p, int maxpoints, const ImVec2* points)
    {
        if (maxpoints < 2 || points == 0)
            return 0;
        if (p < 0)
            return points[0].y;

        int left = 0;
        while (left < maxpoints && points[left].x < p && points[left].x != -1)
            left++;
        if (left)
            left--;

        if (left == maxpoints - 1)
            return points[maxpoints - 1].y;

        float d = (p - points[left].x) / (points[left + 1].x - points[left].x);

        return points[left].y + (points[left + 1].y - points[left].y) * d;
    }

    static inline float ImRemap(float v, float a, float b, float c, float d)
    {
        return (c + (d - c) * (v - a) / (b - a));
    }

    static inline ImVec2 ImRemap(const ImVec2& v, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d)
    {
        return ImVec2(ImRemap(v.x, a.x, b.x, c.x, d.x), ImRemap(v.y, a.y, b.y, c.y, d.y));
    }


    int Curve(const char* label,
        const ImVec2& size,
        const int maxpoints,
        ImVec2* points,
        float traceOffsetCurveValues[256],
        int* selection,
        const ImVec2& rangeMin,
        const ImVec2& rangeMax)
    {
        int modified = 0;
        int i;
        if (maxpoints < 2 || points == nullptr)
            return 0;

        if (points[0].x <= CurveTerminator)
        {
            points[0] = rangeMin;
            points[1] = rangeMax;
            points[2].x = CurveTerminator;
        }

        ImGuiWindow* window = GetCurrentWindow();
        ImGuiContext& g = *GImGui;

        const ImGuiID id = window->GetID(label);
        if (window->SkipItems)
            return 0;

        // The provided size argument represents the max size the widget can reach,
        // but if the window is smaller than the size's x value, then we need to clamp
        // the size argument
        ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2((window->Size.x < size.x) ? window->Size.x : size.x, (window->Size.y < size.y) ? window->Size.y : size.y));
        ItemSize(bb);
        if (!ItemAdd(bb, NULL))
            return 0;

        PushID(label);

        int currentSelection = selection ? *selection : -1;

        const bool hovered = ImGui::ItemHoverable(bb, id);

        int pointCount = 0;
        while (pointCount < maxpoints && points[pointCount].x >= rangeMin.x)
            pointCount++;

        const ImGuiStyle& style = g.Style;
        RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg, 1), true, style.FrameRounding);

        const float ht = bb.Max.y - bb.Min.y;
        const float wd = bb.Max.x - bb.Min.x;

        int hoveredPoint = -1;

        const float pointRadiusInPixels = 5.0f;

        // Handle point selection
        if (hovered)
        {
            ImVec2 hoverPos = (g.IO.MousePos - bb.Min) / (bb.Max - bb.Min);
            hoverPos.y = 1.0f - hoverPos.y;

            ImVec2 pos = ImRemap(hoverPos, ImVec2(0, 0), ImVec2(1, 1), rangeMin, rangeMax);

            int left = 0;
            while (left < pointCount && points[left].x < pos.x)
                left++;
            if (left)
                left--;

            const ImVec2 hoverPosScreen = ImRemap(hoverPos, ImVec2(0, 0), ImVec2(1, 1), bb.Min, bb.Max);
            const ImVec2 p1s = ImRemap(points[left], rangeMin, rangeMax, bb.Min, bb.Max);
            const ImVec2 p2s = ImRemap(points[left + 1], rangeMin, rangeMax, bb.Min, bb.Max);

            const float p1d = ImSqrt(ImLengthSqr(p1s - hoverPosScreen));
            const float p2d = ImSqrt(ImLengthSqr(p2s - hoverPosScreen));

            if (p1d < pointRadiusInPixels)
                hoveredPoint = left;

            if (p2d < pointRadiusInPixels)
                hoveredPoint = left + 1;

            if (g.IO.MouseDown[0])
            {
                if (currentSelection == -1)
                    currentSelection = hoveredPoint;
            }
            else
                currentSelection = -1;

            enum
            {
                action_none,
                action_add_point,
                action_delete_point
            };

            int action = action_none;

            if (currentSelection == -1)
            {
                if (g.IO.MouseDoubleClicked[0] || IsMouseDragging(0))
                    action = action_add_point;
            }
            else if (g.IO.MouseDoubleClicked[0])
                action = action_delete_point;

            if (action == action_add_point)
            {
                if (pointCount < maxpoints)
                {
                    // select
                    currentSelection = left + 1;

                    ++pointCount;
                    for (i = pointCount; i > left; --i)
                        points[i] = points[i - 1];

                    points[left + 1] = pos;

                    if (pointCount < maxpoints)
                        points[pointCount].x = CurveTerminator;
                }
            }
            else if (action == action_delete_point)
            {
                // delete point
                if (currentSelection > 0 && currentSelection < maxpoints - 1)
                {
                    for (i = currentSelection; i < maxpoints - 1; ++i)
                        points[i] = points[i + 1];

                    --pointCount;
                    points[pointCount].x = CurveTerminator;
                    currentSelection = -1;
                }
            }
        }

        // handle point dragging
        const bool draggingPoint = IsMouseDragging(0) && currentSelection != -1;

        if (draggingPoint)
        {
            if (selection)
                SetActiveID(id, window);

            SetFocusID(id, window);
            FocusWindow(window);

            modified = 1;

            ImVec2 pos = (g.IO.MousePos - bb.Min) / (bb.Max - bb.Min);

            // constrain Y to min/max
            pos.y = 1.0f - pos.y;
            pos = ImRemap(pos, ImVec2(0, 0), ImVec2(1, 1), rangeMin, rangeMax);

            // constrain X to the min left/ max right
            const float pointXRangeMin = (currentSelection > 0) ? points[currentSelection - 1].x : rangeMin.x;
            const float pointXRangeMax = (currentSelection + 1 < pointCount) ? points[currentSelection + 1].x : rangeMax.x;

            pos = ImClamp(pos, ImVec2(pointXRangeMin, rangeMin.y), ImVec2(pointXRangeMax, rangeMax.y));

            points[currentSelection] = pos;

            // snap X first/last to min/max
            if (points[0].x < points[pointCount - 1].x)
            {
                points[0].x = rangeMin.y;
                points[pointCount - 1].x = rangeMax.x;
            }
            else
            {
                points[0].x = rangeMax.x;
                points[pointCount - 1].x = rangeMin.y;
            }
        }

        if (!IsMouseDragging(0) && GetActiveID() == id && selection && *selection != -1 && currentSelection == -1)
        {
            ClearActiveID();
        }

        const ImU32 gridColor1 = GetColorU32(ImGuiCol_TextDisabled, 0.5f);
        const ImU32 gridColor2 = GetColorU32(ImGuiCol_TextDisabled, 0.25f);

        ImDrawList* drawList = window->DrawList;
        

        // bg grid
        drawList->AddLine(ImVec2(bb.Min.x, bb.Min.y + ht / 2), ImVec2(bb.Max.x, bb.Min.y + ht / 2), gridColor1, 3);

        drawList->AddLine(ImVec2(bb.Min.x, bb.Min.y + ht / 4), ImVec2(bb.Max.x, bb.Min.y + ht / 4), gridColor1);

        drawList->AddLine(ImVec2(bb.Min.x, bb.Min.y + ht / 4 * 3), ImVec2(bb.Max.x, bb.Min.y + ht / 4 * 3), gridColor1);

        for (i = 0; i < 9; i++)
        {
            drawList->AddLine(ImVec2(bb.Min.x + (wd / 10) * (i + 1), bb.Min.y), ImVec2(bb.Min.x + (wd / 10) * (i + 1), bb.Max.y), gridColor2);
        }

        drawList->PushClipRect(bb.Min, bb.Max);

        // smooth curve
        enum
        {
            smoothness = 256
        }; // the higher the smoother
        for (i = 0; i <= (smoothness - 1); ++i)
        {
            float px = (i + 0) / float(smoothness);
            float qx = (i + 1) / float(smoothness);

            px = ImRemap(px, 0, 1, rangeMin.x, rangeMax.x);
            qx = ImRemap(qx, 0, 1, rangeMin.x, rangeMax.x);

            const float py = CurveValueSmooth(px, maxpoints, points);
            const float qy = CurveValueSmooth(qx, maxpoints, points);

            ImVec2 p = ImRemap(ImVec2(px, py), rangeMin, rangeMax, ImVec2(0, 0), ImVec2(1, 1));
            ImVec2 q = ImRemap(ImVec2(qx, qy), rangeMin, rangeMax, ImVec2(0, 0), ImVec2(1, 1));

            traceOffsetCurveValues[i] = p.y;

            p.y = 1.0f - p.y;
            q.y = 1.0f - q.y;

            p = ImRemap(p, ImVec2(0, 0), ImVec2(1, 1), bb.Min, bb.Max);
            q = ImRemap(q, ImVec2(0, 0), ImVec2(1, 1), bb.Min, bb.Max);

            drawList->AddLine(p, q, GetColorU32(ImGuiCol_PlotHistogram));
        }

        // lines
        for (i = 1; i < pointCount; i++)
        {
            ImVec2 a = ImRemap(points[i - 1], rangeMin, rangeMax, ImVec2(0, 0), ImVec2(1, 1));
            ImVec2 b = ImRemap(points[i], rangeMin, rangeMax, ImVec2(0, 0), ImVec2(1, 1));

            a.y = 1.0f - a.y;
            b.y = 1.0f - b.y;

            a = ImRemap(a, ImVec2(0, 0), ImVec2(1, 1), bb.Min, bb.Max);
            b = ImRemap(b, ImVec2(0, 0), ImVec2(1, 1), bb.Min, bb.Max);

            drawList->AddLine(a, b, GetColorU32(ImGuiCol_PlotLines, 0.5f));
        }

        if (hovered || draggingPoint)
        {
            // control points
            for (i = 0; i < pointCount; i++)
            {
                ImVec2 p = ImRemap(points[i], rangeMin, rangeMax, ImVec2(0, 0), ImVec2(1, 1));
                p.y = 1.0f - p.y;
                p = ImRemap(p, ImVec2(0, 0), ImVec2(1, 1), bb.Min, bb.Max);

                ImVec2 a = p - ImVec2(4, 4);
                ImVec2 b = p + ImVec2(4, 4);
                if (hoveredPoint == i)
                    drawList->AddRect(a, b, GetColorU32(ImGuiCol_PlotLinesHovered));
                else
                    drawList->AddCircle(p, pointRadiusInPixels, GetColorU32(ImGuiCol_PlotLinesHovered));
            }
        }

        drawList->PopClipRect();

        // draw the text at mouse position
        char buf[128];
        const char* str = label;

        if (hovered || draggingPoint)
        {
            ImVec2 pos = (g.IO.MousePos - bb.Min) / (bb.Max - bb.Min);
            pos.y = 1.0f - pos.y;

            pos = ImLerp(rangeMin, rangeMax, pos);

            snprintf(buf, sizeof(buf), "%s (%.2f,%.2f)", label, pos.x, pos.y);
            str = buf;
        }

        RenderTextClipped(ImVec2(bb.Min.x, bb.Min.y + style.FramePadding.y), bb.Max, str, NULL, NULL, ImVec2(0.5f, 0.5f));

        // curve selector
        static const char* items[] = {
            "Custom",

            "Linear",          "Quad in",     "Quad out",   "Quad in  out",  "Cubic in",   "Cubic out",
            "Cubic in  out",   "Quart in",    "Quart out",  "Quart in  out", "Quint in",   "Quint out",
            "Quint in  out",   "Sine in",     "Sine out",   "Sine in  out",  "Expo in",    "Expo out",
            "Expo in  out",    "Circ in",     "Circ out",   "Circ in  out",  "Elastic in", "Elastic out",
            "Elastic in  out", "Back in",     "Back out",   "Back in  out",  "Bounce in",  "Bounce out",
            "Bounce in out",

            "Sine square",     "Exponential",

            "Schubring1",      "Schubring2",  "Schubring3",

            "SinPi2",          "Swing"
        };

        // buttons; @todo: mirror, smooth, tessellate
        if (ImGui::BeginPopupContextItem(label))
        {
            if (ImGui::Selectable("Reset"))
            {
                points[0] = rangeMin;
                points[1] = rangeMax;
                points[2].x = CurveTerminator;
            }
            if (ImGui::Selectable("Flip"))
            {
                for (i = 0; i < pointCount; ++i)
                {
                    const float yVal = 1.0f - ImRemap(points[i].y, rangeMin.y, rangeMax.y, 0, 1);
                    points[i].y = ImRemap(yVal, 0, 1, rangeMin.y, rangeMax.y);
                }
            }
            if (ImGui::Selectable("Mirror"))
            {
                for (int i = 0, j = pointCount - 1; i < j; i++, j--)
                {
                    ImSwap(points[i], points[j]);
                }
                for (i = 0; i < pointCount; ++i)
                {
                    const float xVal = 1.0f - ImRemap(points[i].x, rangeMin.x, rangeMax.x, 0, 1);
                    points[i].x = ImRemap(xVal, 0, 1, rangeMin.x, rangeMax.x);
                }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Presets"))
            {
                ImGui::PushID("curve_items");
                for (int row = 0; row < IM_ARRAYSIZE(items); ++row)
                {
                    if (ImGui::MenuItem(items[row]))
                    {
                        for (i = 0; i < maxpoints; ++i)
                        {
                            const float px = i / float(maxpoints - 1);
                            const float py = float(tween::ease(row - 1, px));

                            points[i] = ImRemap(ImVec2(px, py), ImVec2(0, 0), ImVec2(1, 1), rangeMin, rangeMax);
                        }
                    }
                }
                ImGui::PopID();
                ImGui::EndMenu();
            }


            ImGui::EndPopup();
        }

        PopID();

        if (selection)
        {
            *selection = currentSelection;
        }

        return modified;
    }
}; // namespace ImGui