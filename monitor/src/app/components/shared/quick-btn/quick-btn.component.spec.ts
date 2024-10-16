import { ComponentFixture, TestBed } from '@angular/core/testing';

import { QuickBtnComponent } from './quick-btn.component';

describe('QuickBtnComponent', () => {
  let component: QuickBtnComponent;
  let fixture: ComponentFixture<QuickBtnComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [QuickBtnComponent]
    })
    .compileComponents();
    
    fixture = TestBed.createComponent(QuickBtnComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
