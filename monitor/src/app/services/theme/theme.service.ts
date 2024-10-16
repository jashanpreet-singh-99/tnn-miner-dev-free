import { Injectable, Renderer2, RendererFactory2 } from '@angular/core';

@Injectable({
  providedIn: 'root'
})
export class ThemeService {
  private lightMode: boolean = false;
  private renderer: Renderer2

  private readonly THEME: string = 'theme';
  private readonly LIGHT: string = 'light';
  private readonly DARK: string = 'dark';

  private readonly CSS_THEME_LIGHT = 'light-mode';
  private readonly CSS_THEME_DARK = 'dark-mode';

  constructor(private renderFactory: RendererFactory2) {
    this.renderer = this.renderFactory.createRenderer(null, null);
    this.loadTheme();
  }

  public toggleTheme(): boolean {
    this.lightMode = !this.lightMode;
    this.saveTheme();
    this.applyTheme();
    return this.lightMode;
  }

  public isLightMode(): boolean {
    return this.lightMode;
  }

  private loadTheme(): void {
    const savedTheme = localStorage.getItem(this.THEME);
    this.lightMode = savedTheme == this.LIGHT;
    this.applyTheme();
  }

  private saveTheme(): void {
    const theme = this.lightMode ? this.LIGHT : this.DARK;
    localStorage.setItem(this.THEME, theme);
  }

  private applyTheme(): void {
    const themeClass = this.lightMode ? this.CSS_THEME_LIGHT : this.CSS_THEME_DARK;
    this.renderer.setAttribute(document.body, 'class', themeClass);
  }
}
