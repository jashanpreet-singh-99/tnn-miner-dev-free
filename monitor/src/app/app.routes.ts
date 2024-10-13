import { Routes } from '@angular/router';
import { DashboardComponent } from './components/www/dashboard/dashboard.component';

export const routes: Routes = [
    {path: 'dash', component: DashboardComponent},
    {path: '**', redirectTo:'dash'},
];
