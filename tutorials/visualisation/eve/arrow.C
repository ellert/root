/// \file
/// \ingroup tutorial_eve
/// Demonstrates usage of TEveArrow class.
///
/// \image html eve_arrow.png
/// \macro_code
///
/// \author Alja Mrak-Tadel

void arrow()
{
   gSystem->IgnoreSignal(kSigSegmentationViolation, true);

   TEveManager::Create();

   auto marker = new TEvePointSet(8);
   marker->SetName("Origin marker");
   marker->SetMarkerColor(6);
   marker->SetMarkerStyle(3);
   Float_t a = 10;
   marker->SetPoint(0, a, +a, +a);
   marker->SetPoint(1, a, -a, +a);
   marker->SetPoint(2, -a, -a, +a);
   marker->SetPoint(3, -a, +a, +a);
   marker->SetPoint(4, +a, +a, -a);
   marker->SetPoint(5, +a, -a, -a);
   marker->SetPoint(6, -a, +a, -a);
   marker->SetPoint(7, -a, -a, -a);
   gEve->AddElement(marker);

   auto a1 = new TEveArrow(1., 1., 10., 10., 4., 0.);
   a1->SetMainColor(kBlue);
   a1->SetTubeR(0.02);
   a1->SetPickable(kTRUE);
   gEve->AddElement(a1);
   auto t1 = new TEveText("blue");
   t1->SetFontSize(20);
   TEveVector tv = a1->GetVector() * 0.5f + a1->GetOrigin();
   t1->RefMainTrans().SetPos(tv.Arr());
   a1->AddElement(t1);

   auto a2 = new TEveArrow(20., 1., 10., 3., 0., 4.);
   a2->SetMainColor(kGreen);
   a2->SetPickable(kTRUE);
   gEve->AddElement(a2);

   auto a3 = new TEveArrow(1., 10., 10., 0., 20., 0.);
   a3->SetMainColor(kOrange);
   a3->SetPickable(kTRUE);
   gEve->AddElement(a3);

   gEve->FullRedraw3D(kTRUE);
}
