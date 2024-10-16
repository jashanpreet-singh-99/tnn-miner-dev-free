import { Component, Input } from '@angular/core';

@Component({
  selector: 'quick-btn',
  standalone: true,
  imports: [],
  templateUrl: './quick-btn.component.html',
  styleUrl: './quick-btn.component.scss'
})
export class QuickBtnComponent {
  @Input() selected: boolean = false;
  @Input() icon: string = 'broken_image';
  @Input() label: string = 'Label';

  get getIcon(): string {
    return this.icon;
  }

  get getLabel(): string {
    return this.label;
  }

  isSelected(): boolean {
    return this.selected;
  }
}
