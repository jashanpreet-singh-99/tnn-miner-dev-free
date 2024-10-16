import { Routes } from '@angular/router';
import { DashboardComponent } from './components/www/dashboard/dashboard.component';
import { PaymentComponent } from './components/www/payment/payment.component';
import { HistoryComponent } from './components/www/history/history.component';
import { SettingsComponent } from './components/www/settings/settings.component';
import { Paths } from './paths.enum';

export const routes: Routes = [
    {path: Paths.Dash, component: DashboardComponent},
    {path: Paths.Payment, component: PaymentComponent},
    {path: Paths.History, component: HistoryComponent},
    {path: Paths.Settings, component: SettingsComponent},
    {path: '**', redirectTo: Paths.Dash},
];
