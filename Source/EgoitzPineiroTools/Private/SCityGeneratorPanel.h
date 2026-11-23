#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCityGeneratorTool;

/**
 * Content of the "Generador de ciudades" tab: the six parameters, Generate and Cleanup.
 * The scenario engineers use only this panel; they never open a PCG graph.
 */
class SCityGeneratorPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCityGeneratorPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FCityGeneratorTool>, Tool)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** A label and a spin box bound to one of the tool parameters. */
	template <typename T>
	TSharedRef<SWidget> MakeParameterRow(const FText& Label, const FText& Tooltip, T& Value, T MinValue, T MaxValue, T Delta);
	TSharedRef<SWidget> MakeSectionHeader(const FText& Title) const;

	FReply OnGenerateClicked();
	FReply OnCleanupClicked();
	FText GetGraphStatusText() const;

	TSharedPtr<FCityGeneratorTool> Tool;
	FText StatusText;

	mutable bool bGraphAvailable = false;
	mutable double NextGraphCheckTime = 0.0;
};
