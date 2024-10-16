import { Component, Input, OnInit } from '@angular/core';
import { QuickBtnComponent } from "../quick-btn/quick-btn.component";
import { CommonModule } from '@angular/common';
import { NavigationEnd, Router } from '@angular/router';
import { Paths } from '../../../paths.enum';
import { ThemeService } from '../../../services/theme/theme.service';

type QuickMenu = {
  img: string;
  text: string;
  path: Paths;
  visible: boolean;
}

@Component({
  selector: 'quick-bar',
  standalone: true,
  imports: [QuickBtnComponent, CommonModule],
  templateUrl: './quick-bar.component.html',
  styleUrl: './quick-bar.component.scss'
})
export class QuickBarComponent implements OnInit {
  @Input() menus: QuickMenu[] = [
    {img: 'monitoring', text: 'Dashboard', path: Paths.Dash, visible: true},
    {img: 'account_balance', text: 'Payment', path: Paths.Payment, visible: true},
    {img: 'schedule', text: 'History', path: Paths.History, visible: true},
    {img: 'settings', text: 'Settings', path: Paths.Settings, visible: false}
  ];
  
  private selected: number = -1;
  
  protected lightMode: boolean = true;

  constructor(private router: Router, private themeService: ThemeService) {}
  
  ngOnInit(): void {
    this.lightMode = this.themeService.isLightMode();

    this.router.events.subscribe(event => {
      if (event instanceof NavigationEnd) {
        this.updateNavigation(this.router.url);
      }
    });
    this.updateNavigation(this.router.url);
  }

  get settingsIndex(): number {
    return this.menus.length - 1;
  }

  get visibleMenus(): QuickMenu[] {
    return this.menus.filter((menu: QuickMenu) => menu.visible);
  }

  isSelectedMenu(index: number): boolean {
    return this.selected == index;
  }

  navigateToSelectedMenu(index: number): void {
    this.changeSelectedMenu(index);
    this.router.navigate([this.menus[index].path])
  }

  changeSelectedMenu(index: number) {
    this.selected = index;
  }

  updateNavigation(url: string): void {
    url = url.replaceAll('/', '');
    console.log('Switch check:', url, Paths.Dash);
    switch (url) {
      case Paths.Dash:
        this.changeSelectedMenu(0);
        break;
      case Paths.Payment:
        this.changeSelectedMenu(1);
        break;
      case Paths.History:
        this.changeSelectedMenu(2);
        break;
      case Paths.Settings:
        this.changeSelectedMenu(this.settingsIndex);
        break;
      default:
        this.changeSelectedMenu(-1);
        break;
    }
  }

  isDarkModeActive(): boolean {
    return !this.lightMode;
  }

  switchToDarkMode(): void {
    this.lightMode = this.themeService.toggleTheme();
  }
}
