#pragma once

#include "pch.h"
#include "ExcelStudyRepository.h"
#include "Settings.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace med
{
    class MainWindow
    {
    public:
        MainWindow();
        void Activate();

    private:
        using FrameworkElement = winrt::Microsoft::UI::Xaml::FrameworkElement;
        using StackPanel = winrt::Microsoft::UI::Xaml::Controls::StackPanel;
        using TextBox = winrt::Microsoft::UI::Xaml::Controls::TextBox;
        using ComboBox = winrt::Microsoft::UI::Xaml::Controls::ComboBox;
        using CheckBox = winrt::Microsoft::UI::Xaml::Controls::CheckBox;

        void BuildNavigation();
        void LoadStudies();
        void RenderSection(const std::wstring& tag);
        void RenderOverview();
        void RenderStudies();
        void RenderGlobalReports();
        void RenderGlobalDocuments();
        void RenderSettings();

        FrameworkElement BuildTopBar(const std::wstring& title, const std::wstring& subtitle = L"");
        FrameworkElement BuildStudyTabs();
        FrameworkElement BuildStudyOverview(const StudyData& study);
        FrameworkElement BuildStudyProfile(const StudyData& study);
        FrameworkElement BuildOrders(const StudyData& study);
        FrameworkElement BuildInventory(const StudyData& study);
        FrameworkElement BuildVisits(const StudyData& study);
        FrameworkElement BuildReports(const StudyData& study);
        FrameworkElement BuildDocuments(const StudyData& study);
        FrameworkElement BuildNotes(const StudyData& study);
        FrameworkElement BuildDocumentQuickLinks(const StudyData& study, const std::wstring& categoryContains);
        const ImpData* SelectedImp(const StudyData& study) const;

        const StudyData* CurrentStudy() const;
        StudyData* CurrentStudy();
        std::filesystem::path ReportRoot(const StudyData& study) const;
        std::filesystem::path GlobalReportRoot() const;
        std::filesystem::path DefaultDrugAccountOverallTemplate() const;
        std::filesystem::path DefaultDrugAccountPatientTemplate() const;
        std::optional<std::filesystem::path> PickFolder(const std::filesystem::path& initialFolder) const;
        void ChooseStudyFolder();
        void ReloadKeepingSelection();
        void ReloadCurrentStudyKeepingSelection();
        void OpenPath(const std::filesystem::path& path) const;
        void ShowError(const std::wstring& message) const;
        void ShowError(const std::exception& error) const;
        void ShowInfo(const std::wstring& message) const;
        void RefreshVisitDateInput();
        void RefreshCurrentView();

        struct TableViewState
        {
            std::map<size_t, std::wstring> filters;
            int sortColumn{ -1 };
            bool sortAscending{ true };
            int selectedKey{ -1 };
            double verticalOffset{ 0.0 };
            double horizontalOffset{ 0.0 };
        };

        FrameworkElement BuildInteractiveTable(
            const std::wstring& key,
            const std::vector<std::wstring>& headers,
            const std::vector<std::vector<std::wstring>>& rows,
            const std::vector<int>& rowKeys = {},
            size_t maxRows = 100,
            bool selectable = false);

        std::vector<std::vector<std::wstring>> DisplayRows(const SheetTable& table, size_t maxRows = 100) const;
        std::vector<int> DisplayRowKeys(const SheetTable& table, size_t maxRows = 100) const;
        static std::wstring JoinVisitMetadata(const VisitRowDefinition& row);
        static int HeaderLike(const SheetTable& table, const std::vector<std::wstring>& candidates);

        winrt::Microsoft::UI::Xaml::Window m_window{ nullptr };
        HWND m_hwnd{};
        winrt::Microsoft::UI::Xaml::Controls::NavigationView m_navigation{ nullptr };

        AppSettings m_settings;
        ExcelStudyRepository m_repository;
        std::vector<StudyData> m_studies;
        size_t m_selectedStudy{ 0 };
        size_t m_selectedImp{ 0 };
        int m_selectedStudyTab{ 0 };
        std::wstring m_currentSection{ L"overview" };
        bool m_rendering{ false };
        double m_overviewScrollOffset{ 0.0 };

        ComboBox m_studyCombo{ nullptr };
        ComboBox m_orderImpCombo{ nullptr };
        ComboBox m_inventoryImpCombo{ nullptr };
        ComboBox m_reportImpCombo{ nullptr };
        std::map<std::wstring, TextBox> m_orderInputs;
        std::map<std::wstring, TextBox> m_orderEditInputs;
        std::map<std::wstring, TextBox> m_inventoryEditInputs;
        std::map<std::wstring, TextBox> m_profileRowInputs;
        std::map<std::wstring, TextBox> m_visitMetaInputs;

        ComboBox m_visitPatientCombo{ nullptr };
        ComboBox m_visitRowCombo{ nullptr };
        TextBox m_visitDateInput{ nullptr };
        TextBox m_newPatientInput{ nullptr };
        ComboBox m_reportPatientCombo{ nullptr };
        TextBox m_settingsFolderInput{ nullptr };
        TextBox m_auditFolderInput{ nullptr };
        std::map<std::wstring, TextBox> m_exportFolderInputs;
        TextBox m_notesInput{ nullptr };
        TextBox m_profileStudyNameInput{ nullptr };
        TextBox m_profileEuctInput{ nullptr };

        TextBox m_batchDelivery{ nullptr };
        TextBox m_batchCharge{ nullptr };
        TextBox m_batchExpiry{ nullptr };
        TextBox m_batchCount{ nullptr };
        TextBox m_batchIdentifiers{ nullptr };
        CheckBox m_batchSameIdentifier{ nullptr };
        ComboBox m_batchOrderCombo{ nullptr };
        std::vector<int> m_batchOpenOrderRows;

        std::map<std::wstring, TableViewState> m_tableStates;
    };
}
